[CmdletBinding(DefaultParameterSetName = "Finalize")]
param(
    [Parameter(Mandatory = $true, ParameterSetName = "Finalize")]
    [string]$CandidatePath,
    [Parameter(Mandatory = $true, ParameterSetName = "Finalize")]
    [string]$EvidencePath,
    [Parameter(Mandatory = $true, ParameterSetName = "Finalize")]
    [string]$ExpectedSignerSubject,
    [Parameter(Mandatory = $true, ParameterSetName = "Finalize")]
    [string]$ExpectedUpdateFeedUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Finalize")]
    [string]$ExpectedReleaseBaseUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Finalize")]
    [string]$ExpectedReleasePageBaseUrl,
    [Parameter(Mandatory = $true, ParameterSetName = "Finalize")]
    [string]$ExpectedSignerThumbprint,
    [Parameter(Mandatory = $true, ParameterSetName = "DecisionTest")]
    [switch]$DecisionTestMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName PresentationCore
. (Join-Path $PSScriptRoot "release-path-safety.ps1")
. (Join-Path $PSScriptRoot "git-trust-safety.ps1")
. (Join-Path $PSScriptRoot "runtime-helper-provenance.ps1")

function Get-RequiredAcceptanceCategoryIds {
    return @(
        "system_text_editor",
        "office_word_processor",
        "office_spreadsheet_presentation",
        "browser",
        "instant_messaging",
        "collaboration_office",
        "code_text_editor",
        "pdf_readonly"
    )
}

function Get-RequiredAcceptanceCheckIds {
    return @(
        "selection_toolbar",
        "actions_consent",
        "replace_selection",
        "response_states",
        "screen_layout",
        "privacy_boundaries",
        "late_results_cleanup"
    )
}

function Get-AcceptanceApplicationRule {
    param([Parameter(Mandatory = $true)][string]$CategoryId)

    switch ($CategoryId) {
        "system_text_editor" {
            return [PSCustomObject]@{ Allowed = @("windows_notepad"); RequiredAll = @("windows_notepad"); RequiredAny = @() }
        }
        "office_word_processor" {
            return [PSCustomObject]@{ Allowed = @("microsoft_word", "wps_writer"); RequiredAll = @(); RequiredAny = @("microsoft_word", "wps_writer") }
        }
        "office_spreadsheet_presentation" {
            return [PSCustomObject]@{ Allowed = @("microsoft_excel", "microsoft_powerpoint", "wps_spreadsheet", "wps_presentation"); RequiredAll = @(); RequiredAny = @("microsoft_excel", "microsoft_powerpoint", "wps_spreadsheet", "wps_presentation") }
        }
        "browser" {
            return [PSCustomObject]@{ Allowed = @("microsoft_edge", "google_chrome"); RequiredAll = @("microsoft_edge", "google_chrome"); RequiredAny = @() }
        }
        "instant_messaging" {
            return [PSCustomObject]@{ Allowed = @("wechat"); RequiredAll = @("wechat"); RequiredAny = @() }
        }
        "collaboration_office" {
            return [PSCustomObject]@{ Allowed = @("feishu"); RequiredAll = @("feishu"); RequiredAny = @() }
        }
        "code_text_editor" {
            return [PSCustomObject]@{ Allowed = @("visual_studio_code", "notepad_plus_plus"); RequiredAll = @(); RequiredAny = @("visual_studio_code", "notepad_plus_plus") }
        }
        "pdf_readonly" {
            return [PSCustomObject]@{ Allowed = @("edge_pdf", "adobe_acrobat_reader"); RequiredAll = @(); RequiredAny = @("edge_pdf", "adobe_acrobat_reader") }
        }
        default { throw "Unknown acceptance category application rule: $CategoryId" }
    }
}

function Get-RequiredJsonProperty {
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [string]$Context = "JSON object"
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "$Context is missing required property '$Name'."
    }
    return $property.Value
}

function Assert-NonBlankEvidenceValue {
    param(
        [Parameter(Mandatory = $true)]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Context
    )

    $value = [string](Get-RequiredJsonProperty -Object $Object -Name $Name -Context $Context)
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "$Context property '$Name' must not be blank."
    }
}

function Assert-ReleaseAcceptanceEvidence {
    param(
        [Parameter(Mandatory = $true)]$Evidence,
        [Parameter(Mandatory = $true)]$Candidate
    )

    if ([int](Get-RequiredJsonProperty -Object $Evidence -Name "schema_version" -Context "Acceptance evidence") -ne 1 -or
        [string](Get-RequiredJsonProperty -Object $Evidence -Name "kind" -Context "Acceptance evidence") -cne
            "vocekit-release-acceptance-evidence") {
        throw "Acceptance evidence schema is not supported."
    }
    foreach ($binding in @("source_commit", "version", "tag")) {
        $evidenceValue = [string](Get-RequiredJsonProperty -Object $Evidence -Name $binding -Context "Acceptance evidence")
        $candidateValue = [string](Get-RequiredJsonProperty -Object $Candidate -Name $binding -Context "Candidate record")
        if ($evidenceValue -cne $candidateValue) {
            throw "Acceptance evidence '$binding' does not match the signed candidate."
        }
    }
    if ([string](Get-RequiredJsonProperty -Object $Evidence -Name "overall_status" -Context "Acceptance evidence") -cne "passed") {
        throw "Acceptance evidence overall_status must be 'passed'."
    }

    foreach ($artifactName in @("archive", "sidecar", "manifest")) {
        $evidenceArtifact = Get-RequiredJsonProperty -Object $Evidence -Name $artifactName -Context "Acceptance evidence"
        $candidateArtifact = Get-RequiredJsonProperty -Object $Candidate -Name $artifactName -Context "Candidate record"
        foreach ($binding in @("name", "bytes", "sha256")) {
            $evidenceValue = Get-RequiredJsonProperty -Object $evidenceArtifact -Name $binding -Context "Acceptance $artifactName binding"
            $candidateValue = Get-RequiredJsonProperty -Object $candidateArtifact -Name $binding -Context "Candidate $artifactName binding"
            if ([string]$evidenceValue -cne [string]$candidateValue) {
                throw "Acceptance evidence $artifactName '$binding' does not match the signed candidate."
            }
        }
    }
    $candidateRecordBinding = Get-RequiredJsonProperty `
        -Object $Evidence `
        -Name "candidate_record" `
        -Context "Acceptance evidence"
    if ([string](Get-RequiredJsonProperty -Object $candidateRecordBinding -Name "name" -Context "Acceptance candidate record binding") -cne
        "release-candidate.json" -or
        [long](Get-RequiredJsonProperty -Object $candidateRecordBinding -Name "bytes" -Context "Acceptance candidate record binding") -lt 1 -or
        [string](Get-RequiredJsonProperty -Object $candidateRecordBinding -Name "sha256" -Context "Acceptance candidate record binding") -notmatch
            '^[0-9a-fA-F]{64}$') {
        throw "Acceptance evidence candidate_record binding is invalid."
    }

    $requiredCategories = @(Get-RequiredAcceptanceCategoryIds)
    $requiredChecks = @(Get-RequiredAcceptanceCheckIds)
    $requiredScales = @(100, 125, 150, 200)
    $cells = @(Get-RequiredJsonProperty -Object $Evidence -Name "cells" -Context "Acceptance evidence")
    if ($cells.Count -ne ($requiredCategories.Count * $requiredScales.Count)) {
        throw "Acceptance evidence must contain exactly 32 matrix cells."
    }

    $cellKeys = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $screenshotReferences = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $screenshotHashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($cell in $cells) {
        $categoryId = [string](Get-RequiredJsonProperty -Object $cell -Name "category_id" -Context "Acceptance cell")
        $scalePercent = [int](Get-RequiredJsonProperty -Object $cell -Name "scale_percent" -Context "Acceptance cell")
        if ($requiredCategories -cnotcontains $categoryId -or $requiredScales -notcontains $scalePercent) {
            throw "Acceptance evidence contains an unknown matrix cell: $categoryId/$scalePercent."
        }
        $key = "$categoryId|$scalePercent"
        if (-not $cellKeys.Add($key)) {
            throw "Acceptance evidence contains a duplicate matrix cell: $categoryId/$scalePercent."
        }
        if ([string](Get-RequiredJsonProperty -Object $cell -Name "status" -Context "Acceptance cell $key") -cne "passed") {
            throw "Acceptance cell $key did not pass."
        }
        foreach ($name in @(
            "application_name", "application_version", "windows_version",
            "display_resolution", "monitor_coordinates", "tested_at"
        )) {
            Assert-NonBlankEvidenceValue -Object $cell -Name $name -Context "Acceptance cell $key"
        }

        $applicationRule = Get-AcceptanceApplicationRule -CategoryId $categoryId
        $applications = @(Get-RequiredJsonProperty -Object $cell -Name "applications" -Context "Acceptance cell $key")
        if ($applications.Count -lt 1) {
            throw "Acceptance cell $key must identify its tested applications."
        }
        $applicationIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($application in $applications) {
            $applicationId = [string](Get-RequiredJsonProperty -Object $application -Name "id" -Context "Acceptance cell $key application")
            if ($applicationRule.Allowed -cnotcontains $applicationId -or
                -not $applicationIds.Add($applicationId)) {
                throw "Acceptance cell $key contains an invalid or duplicate application '$applicationId'."
            }
            Assert-NonBlankEvidenceValue -Object $application -Name "name" -Context "Acceptance cell $key application $applicationId"
            Assert-NonBlankEvidenceValue -Object $application -Name "version" -Context "Acceptance cell $key application $applicationId"
        }
        foreach ($requiredApplicationId in $applicationRule.RequiredAll) {
            if (-not $applicationIds.Contains($requiredApplicationId)) {
                throw "Acceptance cell $key is missing required application '$requiredApplicationId'."
            }
        }
        if ($applicationRule.RequiredAny.Count -gt 0 -and
            @($applicationRule.RequiredAny | Where-Object { $applicationIds.Contains($_) }).Count -lt 1) {
            throw "Acceptance cell $key does not contain an approved representative application."
        }
        $derivedApplicationName = @($applications | ForEach-Object { [string]$_.name }) -join " + "
        $derivedApplicationVersion = @($applications | ForEach-Object { [string]$_.version }) -join " + "
        if ([string]$cell.application_name -cne $derivedApplicationName -or
            [string]$cell.application_version -cne $derivedApplicationVersion) {
            throw "Acceptance cell $key application summary does not match its structured applications."
        }
        $testedAtText = [string]$cell.tested_at
        $testedAt = [DateTimeOffset]::MinValue
        if ($testedAtText -cnotmatch '(?:Z|[+-](?:[01][0-9]|2[0-3]):[0-5][0-9])$' -or
            -not [DateTimeOffset]::TryParse(
                $testedAtText,
                [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]::RoundtripKind,
                [ref]$testedAt
            )) {
            throw "Acceptance cell $key has an invalid RFC 3339 tested_at timestamp with an explicit timezone: '$testedAtText'."
        }

        $screenshots = @(Get-RequiredJsonProperty -Object $cell -Name "screenshots" -Context "Acceptance cell $key")
        if ($screenshots.Count -lt 1) {
            throw "Acceptance cell $key must reference at least one screenshot."
        }
        $screenshotApplicationIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($screenshot in $screenshots) {
            $screenshotApplicationId = [string](Get-RequiredJsonProperty `
                -Object $screenshot `
                -Name "application_id" `
                -Context "Acceptance cell $key screenshot")
            if (-not $applicationIds.Contains($screenshotApplicationId)) {
                throw "Acceptance cell $key screenshot references an untested application '$screenshotApplicationId'."
            }
            [void]$screenshotApplicationIds.Add($screenshotApplicationId)
            Assert-NonBlankEvidenceValue -Object $screenshot -Name "reference" -Context "Acceptance cell $key screenshot"
            $screenshotReference = [string]$screenshot.reference
            $screenshotHash = [string](Get-RequiredJsonProperty -Object $screenshot -Name "sha256" -Context "Acceptance cell $key screenshot")
            if ($screenshotHash -notmatch '^[0-9a-fA-F]{64}$') {
                throw "Acceptance cell $key has an invalid screenshot SHA-256."
            }
            if (-not $screenshotReferences.Add($screenshotReference) -or
                -not $screenshotHashes.Add($screenshotHash)) {
                throw "Acceptance cells must not reuse screenshot references or screenshot bytes."
            }
        }
        foreach ($applicationId in $applicationIds) {
            if (-not $screenshotApplicationIds.Contains($applicationId)) {
                throw "Acceptance cell $key is missing screenshot evidence for application '$applicationId'."
            }
        }
        $issues = @(Get-RequiredJsonProperty -Object $cell -Name "issues" -Context "Acceptance cell $key")
        if ($issues.Count -gt 0) {
            throw "Acceptance cell $key still contains unresolved issues."
        }

        $checks = @(Get-RequiredJsonProperty -Object $cell -Name "checks" -Context "Acceptance cell $key")
        if ($checks.Count -ne $requiredChecks.Count) {
            throw "Acceptance cell $key must contain exactly seven checks."
        }
        $checkIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($check in $checks) {
            $checkId = [string](Get-RequiredJsonProperty -Object $check -Name "id" -Context "Acceptance cell $key check")
            if ($requiredChecks -cnotcontains $checkId) {
                throw "Acceptance cell $key contains an unknown check '$checkId'."
            }
            if (-not $checkIds.Add($checkId)) {
                throw "Acceptance cell $key contains duplicate check '$checkId'."
            }
            if ([string](Get-RequiredJsonProperty -Object $check -Name "status" -Context "Acceptance cell $key check $checkId") -cne "passed") {
                throw "Acceptance cell $key check '$checkId' did not pass."
            }
        }
    }
}

function Read-ReleaseJsonFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "$Label is missing: $fullPath"
    }
    try {
        $jsonText = Get-Content -LiteralPath $fullPath -Raw -Encoding UTF8
        $convertFromJson = Get-Command ConvertFrom-Json -ErrorAction Stop
        if ($convertFromJson.Parameters.ContainsKey("DateKind")) {
            # PowerShell 7.5 otherwise converts RFC 3339 strings to DateTime
            # and discards the original explicit timezone spelling before the
            # acceptance evidence validator can enforce it.
            return $jsonText | ConvertFrom-Json -DateKind String -ErrorAction Stop
        }
        return $jsonText | ConvertFrom-Json -ErrorAction Stop
    } catch {
        throw "$Label is not valid JSON: $($_.Exception.Message)"
    }
}

function Assert-SafeReleaseFileName {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ([string]::IsNullOrWhiteSpace($Name) -or
        $Name -cne [IO.Path]::GetFileName($Name) -or
        $Name.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) {
        throw "$Label must be a single safe file name."
    }
}

function Assert-CandidateRecordSchema {
    param(
        [Parameter(Mandatory = $true)]$Candidate,
        [Parameter(Mandatory = $true)][string]$ExpectedSignerSubject,
        [string]$ExpectedSignerThumbprint = "",
        [Parameter(Mandatory = $true)][string]$ExpectedUpdateFeedUrl,
        [Parameter(Mandatory = $true)][string]$ExpectedReleaseBaseUrl,
        [Parameter(Mandatory = $true)][string]$ExpectedReleasePageBaseUrl
    )

    if ([int](Get-RequiredJsonProperty -Object $Candidate -Name "schema_version" -Context "Candidate record") -ne 1 -or
        [string](Get-RequiredJsonProperty -Object $Candidate -Name "kind" -Context "Candidate record") -cne
            "vocekit-signed-release-candidate") {
        throw "Candidate record schema is not supported."
    }
    $createdAt = [DateTimeOffset]::MinValue
    if (-not [DateTimeOffset]::TryParse(
        [string](Get-RequiredJsonProperty -Object $Candidate -Name "created_at" -Context "Candidate record"),
        [ref]$createdAt
    )) {
        throw "Candidate record has an invalid created_at timestamp."
    }
    $sourceCommit = [string](Get-RequiredJsonProperty -Object $Candidate -Name "source_commit" -Context "Candidate record")
    if ($sourceCommit -notmatch '^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$') {
        throw "Candidate record source_commit is not a complete Git object ID."
    }
    $version = [string](Get-RequiredJsonProperty -Object $Candidate -Name "version" -Context "Candidate record")
    if ($version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$') {
        throw "Candidate record version must be a stable x.y.z version."
    }
    $tag = [string](Get-RequiredJsonProperty -Object $Candidate -Name "tag" -Context "Candidate record")
    if ($tag -cne "v$version") {
        throw "Candidate record tag does not match its version."
    }
    $packageName = [string](Get-RequiredJsonProperty -Object $Candidate -Name "package_name" -Context "Candidate record")
    Assert-SafeReleaseFileName -Name $packageName -Label "Candidate package_name"
    $binaryProvenance = Get-RequiredJsonProperty -Object $Candidate -Name "binary_provenance" -Context "Candidate record"
    $binarySourceTreeClean = Get-RequiredJsonProperty `
        -Object $binaryProvenance `
        -Name "source_tree_clean" `
        -Context "Candidate binary provenance"
    if ($binarySourceTreeClean -isnot [bool] -or -not [bool]$binarySourceTreeClean) {
        throw "Candidate binary provenance must confirm a clean Git source tree."
    }
    if ([int](Get-RequiredJsonProperty -Object $binaryProvenance -Name "schema_version" -Context "Candidate binary provenance") -ne 3 -or
        [string](Get-RequiredJsonProperty -Object $binaryProvenance -Name "source_commit" -Context "Candidate binary provenance") -cne $sourceCommit -or
        [string](Get-RequiredJsonProperty -Object $binaryProvenance -Name "configuration" -Context "Candidate binary provenance") -cne "release" -or
        [string](Get-RequiredJsonProperty -Object $binaryProvenance -Name "version" -Context "Candidate binary provenance") -cne $version -or
        [string](Get-RequiredJsonProperty -Object $binaryProvenance -Name "update_feed_url" -Context "Candidate binary provenance") -cne $ExpectedUpdateFeedUrl) {
        throw "Candidate binary provenance does not bind source_commit, release configuration, and version."
    }

    $expectedRuntimeHelpers = @{
        "vocekit-windows-ocr" = "ocr/windows/vocekit-windows-ocr.exe"
        "vocekit-rapidocr" = "ocr/rapidocr/vocekit-rapidocr.exe"
        "vocekit-windows-speech" = "speech/windows/vocekit-windows-speech.exe"
    }
    $runtimeHelpersValue = Get-RequiredJsonProperty `
        -Object $Candidate `
        -Name "runtime_helpers" `
        -Context "Candidate record"
    $runtimeHelpers = @($runtimeHelpersValue)
    if ($runtimeHelpers.Count -ne $expectedRuntimeHelpers.Count) {
        throw "Candidate record must bind exactly three publisher-owned runtime helpers."
    }
    $seenRuntimeHelpers = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($binding in $runtimeHelpers) {
        $helperName = [string](Get-RequiredJsonProperty `
            -Object $binding `
            -Name "helper_name" `
            -Context "Candidate runtime helper binding")
        $relativePath = [string](Get-RequiredJsonProperty `
            -Object $binding `
            -Name "relative_path" `
            -Context "Candidate runtime helper binding")
        $sha256 = [string](Get-RequiredJsonProperty `
            -Object $binding `
            -Name "sha256" `
            -Context "Candidate runtime helper binding")
        if (-not $expectedRuntimeHelpers.ContainsKey($helperName) -or
            -not $seenRuntimeHelpers.Add($helperName) -or
            $relativePath -cne [string]$expectedRuntimeHelpers[$helperName] -or
            $sha256 -notmatch '^[0-9a-f]{64}$') {
            throw "Candidate runtime helper binding is missing, duplicated, or malformed."
        }
        $helperProvenance = Get-RequiredJsonProperty `
            -Object $binding `
            -Name "provenance" `
            -Context "Candidate runtime helper binding"
        Assert-RuntimeHelperBuildProvenanceObject `
            -Provenance $helperProvenance `
            -ExpectedHelperName $helperName `
            -ExpectedSourceCommit $sourceCommit `
            -ExpectedSourceTreeClean $true `
            -ExpectedConfiguration "Release"
    }

    $urls = Get-RequiredJsonProperty -Object $Candidate -Name "urls" -Context "Candidate record"
    $expectedUrls = @{
        update_feed_url = $ExpectedUpdateFeedUrl
        release_base_url = $ExpectedReleaseBaseUrl.TrimEnd("/")
        release_page_base_url = $ExpectedReleasePageBaseUrl.TrimEnd("/")
    }
    foreach ($urlName in @("update_feed_url", "release_base_url", "release_page_base_url")) {
        $actualUrl = [string](Get-RequiredJsonProperty -Object $urls -Name $urlName -Context "Candidate release URLs")
        [void](Get-ReleaseHttpsUri -Value $actualUrl -Label "Candidate $urlName")
        if ($actualUrl -cne [string]$expectedUrls[$urlName]) {
            throw "Candidate $urlName does not match the independently approved release URL."
        }
    }

    $archive = Get-RequiredJsonProperty -Object $Candidate -Name "archive" -Context "Candidate record"
    $sidecar = Get-RequiredJsonProperty -Object $Candidate -Name "sidecar" -Context "Candidate record"
    $manifest = Get-RequiredJsonProperty -Object $Candidate -Name "manifest" -Context "Candidate record"
    if ([string](Get-RequiredJsonProperty -Object $archive -Name "name" -Context "Candidate archive") -cne "$packageName.zip" -or
        [string](Get-RequiredJsonProperty -Object $sidecar -Name "name" -Context "Candidate sidecar") -cne "$packageName.zip.sha256" -or
        [string](Get-RequiredJsonProperty -Object $manifest -Name "name" -Context "Candidate manifest") -cne "update-manifest.json") {
        throw "Candidate artifact names do not match package_name."
    }
    foreach ($entry in @(
        @{ Label = "Candidate archive"; Value = $archive },
        @{ Label = "Candidate sidecar"; Value = $sidecar },
        @{ Label = "Candidate manifest"; Value = $manifest }
    )) {
        Assert-SafeReleaseFileName `
            -Name ([string](Get-RequiredJsonProperty -Object $entry.Value -Name "name" -Context $entry.Label)) `
            -Label "$($entry.Label) name"
        if ([long](Get-RequiredJsonProperty -Object $entry.Value -Name "bytes" -Context $entry.Label) -lt 1) {
            throw "$($entry.Label) bytes must be positive."
        }
        if ([string](Get-RequiredJsonProperty -Object $entry.Value -Name "sha256" -Context $entry.Label) -notmatch '^[0-9a-fA-F]{64}$') {
            throw "$($entry.Label) SHA-256 is invalid."
        }
    }

    $signer = Get-RequiredJsonProperty -Object $Candidate -Name "signer" -Context "Candidate record"
    $signerSubject = [string](Get-RequiredJsonProperty -Object $signer -Name "subject" -Context "Candidate signer")
    $signerThumbprint = ([string](Get-RequiredJsonProperty -Object $signer -Name "thumbprint" -Context "Candidate signer") -replace '\s', '').ToUpperInvariant()
    $timestampSubject = [string](Get-RequiredJsonProperty -Object $signer -Name "timestamp_subject" -Context "Candidate signer")
    $timestampThumbprint = ([string](Get-RequiredJsonProperty -Object $signer -Name "timestamp_thumbprint" -Context "Candidate signer") -replace '\s', '').ToUpperInvariant()
    if ([string]::IsNullOrWhiteSpace($signerSubject) -or
        [string]::IsNullOrWhiteSpace($timestampSubject) -or
        $signerThumbprint -notmatch '^[0-9A-F]{40,64}$' -or
        $timestampThumbprint -notmatch '^[0-9A-F]{40,64}$') {
        throw "Candidate signer identity or RFC 3161 timestamp identity is invalid."
    }
    if (-not [string]::Equals($signerSubject, $ExpectedSignerSubject, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Candidate signer subject does not match the expected publisher."
    }
    $normalizedExpectedThumbprint = ($ExpectedSignerThumbprint -replace '\s', '').ToUpperInvariant()
    if ($normalizedExpectedThumbprint -notmatch '^[0-9A-F]{40,64}$') {
        throw "Expected signer thumbprint must be a complete certificate thumbprint."
    }
    if ($signerThumbprint -cne $normalizedExpectedThumbprint) {
        throw "Candidate signer thumbprint does not match the expected publisher."
    }
}

function Assert-ExtractedRuntimeHelperBindings {
    param(
        [Parameter(Mandatory = $true)][string]$RuntimeDirectory,
        [Parameter(Mandatory = $true)][object[]]$Bindings,
        [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit,
        [Parameter(Mandatory = $true)][string]$VerifierPath
    )

    $runtimeFull = [IO.Path]::GetFullPath($RuntimeDirectory).TrimEnd("\", "/")
    $runtimePrefix = $runtimeFull + [IO.Path]::DirectorySeparatorChar
    foreach ($binding in $Bindings) {
        $relativePath = [string]$binding.relative_path
        $helperPath = [IO.Path]::GetFullPath((Join-Path $runtimeFull $relativePath.Replace("/", "\")))
        if (-not $helperPath.StartsWith($runtimePrefix, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-Path -LiteralPath $helperPath -PathType Leaf)) {
            throw "Extracted runtime helper is missing or escaped the runtime root: $relativePath"
        }
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path $helperPath `
            -Label "Extracted runtime helper")
        $helperItem = Get-Item -LiteralPath $helperPath -Force
        if (($helperItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Extracted runtime helper must not be a reparse point: $relativePath"
        }
        $actualSha256 = (Get-FileHash -LiteralPath $helperPath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualSha256 -cne [string]$binding.sha256) {
            throw "Extracted runtime helper bytes do not match the immutable candidate binding: $relativePath"
        }
        [void](& $VerifierPath `
            -ExecutablePath $helperPath `
            -ExpectedHelperName ([string]$binding.helper_name) `
            -ExpectedSourceCommit $ExpectedSourceCommit)
    }
}

function Assert-ReleaseArtifactBinding {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$Binding,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "$Label is missing: $fullPath"
    }
    $item = Get-Item -LiteralPath $fullPath
    $expectedName = [string](Get-RequiredJsonProperty -Object $Binding -Name "name" -Context "$Label binding")
    $expectedBytes = [long](Get-RequiredJsonProperty -Object $Binding -Name "bytes" -Context "$Label binding")
    $expectedHash = [string](Get-RequiredJsonProperty -Object $Binding -Name "sha256" -Context "$Label binding")
    $actualHash = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash
    if ($item.Name -cne $expectedName -or
        [long]$item.Length -ne $expectedBytes -or
        $actualHash -cne $expectedHash.ToUpperInvariant()) {
        throw "$Label does not match the immutable candidate byte binding."
    }
}

function Assert-ReleaseChecksumSidecar {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$SidecarPath
    )

    $archiveName = [IO.Path]::GetFileName($ArchivePath)
    $archiveHash = (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash
    $sidecarLines = @(
        Get-Content -LiteralPath $SidecarPath -Encoding ASCII |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )
    if ($sidecarLines.Count -ne 1 -or
        $sidecarLines[0] -notmatch '^([0-9a-fA-F]{64})[ \t]+\*?(.+?)\s*$' -or
        $Matches[1].ToUpperInvariant() -cne $archiveHash -or
        $Matches[2] -cne $archiveName) {
        throw "SHA-256 sidecar does not exactly bind the candidate archive."
    }
}

function Get-ReleaseHttpsUri {
    param(
        [Parameter(Mandatory = $true)][string]$Value,
        [Parameter(Mandatory = $true)][string]$Label
    )

    try { $uri = [Uri]$Value } catch { throw "$Label is not a valid absolute URL." }
    if (-not $uri.IsAbsoluteUri -or $uri.Scheme -ne "https" -or
        [string]::IsNullOrWhiteSpace($uri.Host) -or
        -not [string]::IsNullOrWhiteSpace($uri.UserInfo)) {
        throw "$Label must be an absolute HTTPS URL without embedded credentials."
    }
    return $uri
}

function Assert-ReleaseUpdateManifest {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)]$Candidate
    )

    $manifest = Read-ReleaseJsonFile -Path $ManifestPath -Label "Update manifest"
    $archive = Get-RequiredJsonProperty -Object $Candidate -Name "archive" -Context "Candidate record"
    $version = [string]$Candidate.version
    $tag = [string]$Candidate.tag
    $archiveName = [string]$archive.name
    $archiveHash = [string]$archive.sha256
    $releaseUrls = Get-RequiredJsonProperty -Object $Candidate -Name "urls" -Context "Candidate record"
    if ([int](Get-RequiredJsonProperty -Object $manifest -Name "schema_version" -Context "Update manifest") -ne 1 -or
        [string](Get-RequiredJsonProperty -Object $manifest -Name "version" -Context "Update manifest") -cne $version -or
        [string](Get-RequiredJsonProperty -Object $manifest -Name "channel" -Context "Update manifest") -cne "stable" -or
        [bool](Get-RequiredJsonProperty -Object $manifest -Name "prerelease" -Context "Update manifest") -or
        [string](Get-RequiredJsonProperty -Object $manifest -Name "asset_name" -Context "Update manifest") -cne $archiveName -or
        [string](Get-RequiredJsonProperty -Object $manifest -Name "sha256" -Context "Update manifest") -cne $archiveHash) {
        throw "Update manifest does not match the stable signed candidate."
    }
    $publishedAt = [DateTimeOffset]::MinValue
    if (-not [DateTimeOffset]::TryParse(
        [string](Get-RequiredJsonProperty -Object $manifest -Name "published_at" -Context "Update manifest"),
        [ref]$publishedAt
    )) {
        throw "Update manifest published_at is invalid."
    }
    $downloadUrl = [string](Get-RequiredJsonProperty -Object $manifest -Name "download_url" -Context "Update manifest")
    $checksumUrl = [string](Get-RequiredJsonProperty -Object $manifest -Name "checksum_url" -Context "Update manifest")
    $releasePageUrl = [string](Get-RequiredJsonProperty -Object $manifest -Name "release_page_url" -Context "Update manifest")
    $downloadUri = Get-ReleaseHttpsUri -Value $downloadUrl -Label "Update download_url"
    $releasePageUri = Get-ReleaseHttpsUri -Value $releasePageUrl -Label "Update release_page_url"
    [void](Get-ReleaseHttpsUri -Value $checksumUrl -Label "Update checksum_url")
    $expectedDownloadUrl = ([string]$releaseUrls.release_base_url).TrimEnd("/") + "/$tag/$archiveName"
    $expectedReleasePageUrl = ([string]$releaseUrls.release_page_base_url).TrimEnd("/") + "/$tag"
    if ($downloadUrl -cne $expectedDownloadUrl -or
        $checksumUrl -cne "$expectedDownloadUrl.sha256" -or
        $releasePageUrl -cne $expectedReleasePageUrl -or
        -not $downloadUri.AbsolutePath.EndsWith("/$tag/$archiveName", [StringComparison]::Ordinal) -or
        -not $releasePageUri.AbsolutePath.EndsWith("/$tag", [StringComparison]::Ordinal)) {
        throw "Update manifest URLs do not target the candidate tag and archive."
    }
}

function Test-IsWindowsReservedPathSegment {
    param([Parameter(Mandatory = $true)][string]$Segment)

    return $Segment -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?$'
}

function Get-SafeEvidenceAttachmentPath {
    param(
        [Parameter(Mandatory = $true)][string]$EvidenceDirectory,
        [Parameter(Mandatory = $true)][string]$Reference
    )

    $normalized = $Reference.Replace("\", "/")
    $segments = @($normalized.Split('/') | Where-Object { $_.Length -gt 0 })
    if ([string]::IsNullOrWhiteSpace($normalized) -or
        [IO.Path]::IsPathRooted($Reference) -or
        $normalized.StartsWith("/", [StringComparison]::Ordinal) -or
        $segments.Count -eq 0) {
        throw "Evidence screenshot reference is not a safe relative path: $Reference"
    }
    foreach ($segment in $segments) {
        if ($segment -in @(".", "..") -or
            $segment -match '[<>:"|?*\x00-\x1F]' -or
            $segment.EndsWith(" ", [StringComparison]::Ordinal) -or
            $segment.EndsWith(".", [StringComparison]::Ordinal) -or
            (Test-IsWindowsReservedPathSegment -Segment $segment)) {
            throw "Evidence screenshot reference is not a safe relative path: $Reference"
        }
    }
    if ([IO.Path]::GetExtension($segments[-1]).ToLowerInvariant() -notin @(".png", ".jpg", ".jpeg", ".webp")) {
        throw "Evidence screenshot must use a supported image file extension: $Reference"
    }

    $evidenceFull = [IO.Path]::GetFullPath($EvidenceDirectory).TrimEnd("\", "/")
    $targetFull = [IO.Path]::GetFullPath((Join-Path $evidenceFull ($segments -join "\")))
    $evidencePrefix = $evidenceFull + "\"
    if (-not $targetFull.StartsWith($evidencePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Evidence screenshot escaped its evidence directory: $Reference"
    }
    return $targetFull
}

function Assert-SupportedEvidenceImageFile {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    $item = Get-Item -LiteralPath $fullPath -Force -ErrorAction Stop
    if ([long]$item.Length -lt 4 -or [long]$item.Length -gt 52428800L) {
        throw "Evidence screenshot file size is invalid: $fullPath"
    }
    $headerLength = [int][math]::Min(32L, [long]$item.Length)
    $header = New-Object byte[] $headerLength
    $tail = New-Object byte[] 2
    $stream = [IO.File]::Open($fullPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $offset = 0
        while ($offset -lt $header.Length) {
            $read = $stream.Read($header, $offset, $header.Length - $offset)
            if ($read -le 0) { break }
            $offset += $read
        }
        [void]$stream.Seek(-2, [IO.SeekOrigin]::End)
        if ($stream.Read($tail, 0, 2) -ne 2) {
            throw "Evidence screenshot could not be read: $fullPath"
        }
    } finally {
        $stream.Dispose()
    }

    $extension = [IO.Path]::GetExtension($fullPath).ToLowerInvariant()
    $valid = $false
    if ($extension -eq ".png" -and $header.Length -ge 24) {
        $pngSignature = @(0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)
        $signatureMatches = $true
        for ($index = 0; $index -lt $pngSignature.Count; $index++) {
            if ($header[$index] -ne $pngSignature[$index]) { $signatureMatches = $false }
        }
        [uint32]$width = ([uint32]$header[16] -shl 24) -bor ([uint32]$header[17] -shl 16) -bor
            ([uint32]$header[18] -shl 8) -bor [uint32]$header[19]
        [uint32]$height = ([uint32]$header[20] -shl 24) -bor ([uint32]$header[21] -shl 16) -bor
            ([uint32]$header[22] -shl 8) -bor [uint32]$header[23]
        $valid = $signatureMatches -and
            [Text.Encoding]::ASCII.GetString($header, 12, 4) -ceq "IHDR" -and
            $width -gt 0 -and $height -gt 0
    } elseif ($extension -in @(".jpg", ".jpeg")) {
        $valid = $header.Length -ge 4 -and
            $header[0] -eq 0xFF -and $header[1] -eq 0xD8 -and $header[2] -eq 0xFF -and
            $tail[0] -eq 0xFF -and $tail[1] -eq 0xD9
    } elseif ($extension -eq ".webp" -and $header.Length -ge 16) {
        [uint32]$declaredSize = [BitConverter]::ToUInt32($header, 4)
        $chunkType = [Text.Encoding]::ASCII.GetString($header, 12, 4)
        $valid = [Text.Encoding]::ASCII.GetString($header, 0, 4) -ceq "RIFF" -and
            [Text.Encoding]::ASCII.GetString($header, 8, 4) -ceq "WEBP" -and
            $chunkType -in @("VP8 ", "VP8L", "VP8X") -and
            ([long]$declaredSize + 8L) -eq [long]$item.Length
    }
    if (-not $valid) {
        throw "Evidence screenshot is not a supported image file: $fullPath"
    }

    # File signatures alone do not prove that an attachment is a usable
    # screenshot. Decode every pixel through Windows Imaging Component (via
    # WPF), constrain the decoded size before allocating, and reject tiny
    # placeholders that cannot substantiate a desktop acceptance result.
    $decodeStream = [IO.File]::Open(
        $fullPath,
        [IO.FileMode]::Open,
        [IO.FileAccess]::Read,
        [IO.FileShare]::Read
    )
    try {
        try {
            $decoder = [Windows.Media.Imaging.BitmapDecoder]::Create(
                $decodeStream,
                [Windows.Media.Imaging.BitmapCreateOptions]::PreservePixelFormat,
                [Windows.Media.Imaging.BitmapCacheOption]::OnDemand
            )
            if ($decoder.Frames.Count -lt 1) {
                throw "The image decoder returned no frames."
            }
            $frame = $decoder.Frames[0]
            $decodedWidth = [int]$frame.PixelWidth
            $decodedHeight = [int]$frame.PixelHeight
        } catch {
            throw "Evidence screenshot could not be decoded as an image: $fullPath ($($_.Exception.Message))"
        }

        $decodedPixelCount = [long]$decodedWidth * [long]$decodedHeight
        if ($decodedWidth -lt 320 -or
            $decodedHeight -lt 180 -or
            $decodedWidth -gt 8192 -or
            $decodedHeight -gt 8192 -or
            $decodedPixelCount -gt 20000000L) {
            throw "Evidence screenshot decoded dimensions are outside the supported desktop evidence range: $decodedWidth x $decodedHeight ($fullPath)"
        }

        try {
            $decodedBitmap = [Windows.Media.Imaging.FormatConvertedBitmap]::new(
                $frame,
                [Windows.Media.PixelFormats]::Bgra32,
                $null,
                0.0
            )
            $decodedStride = [int]([long]$decodedWidth * 4L)
            $decodedBytes = New-Object byte[] ([int]([long]$decodedStride * [long]$decodedHeight))
            $decodedBitmap.CopyPixels($decodedBytes, $decodedStride, 0)
        } catch {
            throw "Evidence screenshot pixel data could not be fully decoded: $fullPath ($($_.Exception.Message))"
        }
    } finally {
        $decodeStream.Dispose()
    }
}

function Get-VerifiedEvidenceAttachmentPaths {
    param(
        [Parameter(Mandatory = $true)]$Evidence,
        [Parameter(Mandatory = $true)][string]$EvidencePath
    )

    $evidenceDirectory = Split-Path -Parent ([IO.Path]::GetFullPath($EvidencePath))
    [void](Assert-NoReparsePointsInExistingPathChain `
        -Path $evidenceDirectory `
        -Label "Acceptance evidence directory")
    $verified = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($cell in @($Evidence.cells)) {
        foreach ($screenshot in @($cell.screenshots)) {
            $reference = [string]$screenshot.reference
            $attachmentPath = Get-SafeEvidenceAttachmentPath `
                -EvidenceDirectory $evidenceDirectory `
                -Reference $reference
            if (-not $verified.Add($attachmentPath)) {
                throw "Evidence screenshot file is referenced more than once: $reference"
            }
            if (-not (Test-Path -LiteralPath $attachmentPath -PathType Leaf)) {
                throw "Evidence screenshot file is missing: $attachmentPath"
            }
            [void](Assert-NoReparsePointsInExistingPathChain `
                -Path $attachmentPath `
                -Label "Evidence screenshot")
            $item = Get-Item -LiteralPath $attachmentPath -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Evidence screenshot must not be a reparse point: $attachmentPath"
            }
            Assert-SupportedEvidenceImageFile -Path $attachmentPath
            $actualHash = (Get-FileHash -LiteralPath $attachmentPath -Algorithm SHA256).Hash
            if ($actualHash -cne ([string]$screenshot.sha256).ToUpperInvariant()) {
                throw "Evidence screenshot bytes do not match their SHA-256: $reference"
            }
            $attachmentPath
        }
    }
}

function Confirm-ReleaseCandidateMetadata {
    param(
        [Parameter(Mandatory = $true)][string]$CandidatePath,
        [Parameter(Mandatory = $true)][string]$EvidencePath,
        [Parameter(Mandatory = $true)][string]$ExpectedSignerSubject,
        [string]$ExpectedSignerThumbprint = "",
        [Parameter(Mandatory = $true)][string]$ExpectedUpdateFeedUrl,
        [Parameter(Mandatory = $true)][string]$ExpectedReleaseBaseUrl,
        [Parameter(Mandatory = $true)][string]$ExpectedReleasePageBaseUrl,
        [string]$ProjectRoot = ""
    )

    $candidateFull = [IO.Path]::GetFullPath($CandidatePath)
    $evidenceFull = [IO.Path]::GetFullPath($EvidencePath)
    [void](Assert-NoReparsePointsInExistingPathChain `
        -Path $candidateFull `
        -Label "Release candidate record")
    [void](Assert-NoReparsePointsInExistingPathChain `
        -Path $evidenceFull `
        -Label "Acceptance evidence")
    if ([IO.Path]::GetFileName($candidateFull) -cne "release-candidate.json") {
        throw "CandidatePath must point to release-candidate.json."
    }
    $candidateDirectory = [IO.Path]::GetFullPath((Split-Path -Parent $candidateFull))
    $candidatePrefix = $candidateDirectory.TrimEnd("\") + "\"
    if ($evidenceFull.StartsWith($candidatePrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Acceptance evidence must remain external to the immutable candidate directory."
    }

    # The external evidence independently anchors the candidate JSON bytes.
    # Validate that binding before trusting any field parsed from the candidate.
    $evidence = Read-ReleaseJsonFile -Path $evidenceFull -Label "Acceptance evidence"
    $candidateRecordBinding = Get-RequiredJsonProperty `
        -Object $evidence `
        -Name "candidate_record" `
        -Context "Acceptance evidence"
    Assert-ReleaseArtifactBinding `
        -Path $candidateFull `
        -Binding $candidateRecordBinding `
        -Label "Candidate record"

    $candidate = Read-ReleaseJsonFile -Path $candidateFull -Label "Candidate record"
    Assert-CandidateRecordSchema `
        -Candidate $candidate `
        -ExpectedSignerSubject $ExpectedSignerSubject `
        -ExpectedSignerThumbprint $ExpectedSignerThumbprint `
        -ExpectedUpdateFeedUrl $ExpectedUpdateFeedUrl `
        -ExpectedReleaseBaseUrl $ExpectedReleaseBaseUrl `
        -ExpectedReleasePageBaseUrl $ExpectedReleasePageBaseUrl

    if ([IO.Path]::GetFileName($candidateDirectory) -cne [string]$candidate.tag) {
        throw "Candidate record is not inside its version-isolated tag directory."
    }
    if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
        $projectFull = [IO.Path]::GetFullPath($ProjectRoot)
        $expectedCandidatePath = [IO.Path]::GetFullPath((Join-Path $projectFull (
            "dist\releases\$([string]$candidate.tag)\release-candidate.json"
        )))
        if ($candidateFull -cne $expectedCandidatePath) {
            throw "CandidatePath must use the canonical project dist/releases/<tag>/release-candidate.json location."
        }
    }
    Assert-ReleaseAcceptanceEvidence -Evidence $evidence -Candidate $candidate
    $evidenceAttachmentPaths = @(
        Get-VerifiedEvidenceAttachmentPaths `
            -Evidence $evidence `
            -EvidencePath $evidenceFull
    )

    $archive = $candidate.archive
    $sidecar = $candidate.sidecar
    $manifest = $candidate.manifest
    $archivePath = Join-Path $candidateDirectory ([string]$archive.name)
    $sidecarPath = Join-Path $candidateDirectory ([string]$sidecar.name)
    $manifestPath = Join-Path $candidateDirectory ([string]$manifest.name)
    foreach ($artifactPath in @($archivePath, $sidecarPath, $manifestPath)) {
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path $artifactPath `
            -Label "Candidate release artifact")
    }
    Assert-ReleaseArtifactBinding -Path $archivePath -Binding $archive -Label "Candidate archive"
    Assert-ReleaseArtifactBinding -Path $sidecarPath -Binding $sidecar -Label "Candidate sidecar"
    Assert-ReleaseArtifactBinding -Path $manifestPath -Binding $manifest -Label "Candidate manifest"
    Assert-ReleaseChecksumSidecar -ArchivePath $archivePath -SidecarPath $sidecarPath
    Assert-ReleaseUpdateManifest -ManifestPath $manifestPath -Candidate $candidate

    return [PSCustomObject]@{
        Candidate = $candidate
        Evidence = $evidence
        CandidatePath = $candidateFull
        EvidencePath = $evidenceFull
        ArchivePath = [IO.Path]::GetFullPath($archivePath)
        SidecarPath = [IO.Path]::GetFullPath($sidecarPath)
        ManifestPath = [IO.Path]::GetFullPath($manifestPath)
        EvidenceAttachmentPaths = $evidenceAttachmentPaths
    }
}

function Expand-VerifiedReleaseArchive {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [ValidateRange(1, 50000)][int]$MaxEntryCount = 10000,
        [ValidateRange(1, 8589934592L)][long]$MaxTotalUncompressedBytes = 4294967296L,
        [ValidateRange(1, 4294967296L)][long]$MaxSingleFileBytes = 2147483648L,
        [ValidateRange(1, 10000)][int]$MaxCompressionRatio = 1000
    )

    $archiveFull = [IO.Path]::GetFullPath($ArchivePath)
    $destinationFull = [IO.Path]::GetFullPath($DestinationPath)
    if (-not (Test-Path -LiteralPath $archiveFull -PathType Leaf)) {
        throw "Candidate archive is missing: $archiveFull"
    }
    if (Test-Path -LiteralPath $destinationFull) {
        throw "Temporary extraction destination already exists: $destinationFull"
    }
    [void](Assert-NoReparsePointsInExistingPathChain `
        -Path $destinationFull `
        -Label "Temporary extraction directory")

    $destinationPrefix = $destinationFull.TrimEnd("\") + "\"
    $targetPaths = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::OrdinalIgnoreCase)
    $fileCount = 0
    [long]$totalUncompressedBytes = 0
    $archive = [IO.Compression.ZipFile]::OpenRead($archiveFull)
    try {
        if ($archive.Entries.Count -gt $MaxEntryCount) {
            throw "Candidate archive exceeds the maximum entry count of $MaxEntryCount."
        }
        foreach ($entry in $archive.Entries) {
            $normalized = $entry.FullName.Replace("\", "/")
            $segments = @($normalized.Split('/') | Where-Object { $_.Length -gt 0 })
            $unixFileType = ($entry.ExternalAttributes -shr 16) -band 0xF000
            $unsafe = [string]::IsNullOrWhiteSpace($normalized) -or
                $normalized.StartsWith("/", [StringComparison]::Ordinal) -or
                $normalized -match '^[A-Za-z]:' -or
                $unixFileType -eq 0xA000
            foreach ($segment in $segments) {
                if ($segment -in @(".", "..") -or
                    $segment -match '[<>:"|?*\x00-\x1F]' -or
                    $segment.EndsWith(" ", [StringComparison]::Ordinal) -or
                    $segment.EndsWith(".", [StringComparison]::Ordinal) -or
                    (Test-IsWindowsReservedPathSegment -Segment $segment)) {
                    $unsafe = $true
                }
            }
            if ($unsafe -or $segments.Count -eq 0) {
                throw "Candidate archive contains an unsafe entry path: $normalized"
            }

            $targetPath = [IO.Path]::GetFullPath((Join-Path $destinationFull ($segments -join "\")))
            if (-not $targetPath.StartsWith($destinationPrefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Candidate archive contains an unsafe entry path: $normalized"
            }
            $entryType = if ([string]::IsNullOrEmpty($entry.Name)) { "directory" } else { "file" }
            if ($targetPaths.ContainsKey($targetPath)) {
                throw "Candidate archive contains a duplicate or colliding entry: $normalized"
            }
            $targetPaths.Add($targetPath, $entryType)
            if ($entryType -eq "file") {
                if ([long]$entry.Length -gt $MaxSingleFileBytes) {
                    throw "Candidate archive file exceeds the maximum uncompressed size: $normalized"
                }
                $totalUncompressedBytes += [long]$entry.Length
                if ($totalUncompressedBytes -gt $MaxTotalUncompressedBytes) {
                    throw "Candidate archive exceeds the maximum total uncompressed size."
                }
                if ([long]$entry.Length -gt 0 -and
                    ([long]$entry.CompressedLength -le 0 -or
                        ([double]$entry.Length / [math]::Max(1.0, [double]$entry.CompressedLength)) -gt $MaxCompressionRatio)) {
                    throw "Candidate archive entry exceeds the maximum compression ratio: $normalized"
                }
                $fileCount++
            }
        }
    } finally {
        $archive.Dispose()
    }
    if ($fileCount -lt 1) {
        throw "Candidate archive does not contain any files."
    }

    try {
        [void][IO.Directory]::CreateDirectory($destinationFull)
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path $destinationFull `
            -Label "Temporary extraction directory")
        [IO.Compression.ZipFile]::ExtractToDirectory($archiveFull, $destinationFull)
        $extractedFiles = @(Get-ChildItem -LiteralPath $destinationFull -Recurse -File -Force)
        $reparsePoints = @(Get-ChildItem -LiteralPath $destinationFull -Recurse -Force | Where-Object {
            ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
        })
        [long]$actualBytes = ($extractedFiles | Measure-Object -Property Length -Sum).Sum
        if ($reparsePoints.Count -gt 0 -or
            $extractedFiles.Count -ne $fileCount -or
            $actualBytes -ne $totalUncompressedBytes) {
            throw "Extracted candidate runtime does not match the validated ZIP entry set."
        }
    } catch {
        if ([IO.Directory]::Exists($destinationFull)) {
            [IO.Directory]::Delete($destinationFull, $true)
        }
        throw
    }
}

function Assert-NoUnsignedTestMarker {
    param([Parameter(Mandatory = $true)][string]$RuntimeDirectory)

    $runtimeFull = [IO.Path]::GetFullPath($RuntimeDirectory)
    if (-not (Test-Path -LiteralPath $runtimeFull -PathType Container)) {
        throw "Extracted runtime directory is missing: $runtimeFull"
    }
    $markers = @(Get-ChildItem -LiteralPath $runtimeFull -Recurse -File | Where-Object {
        $_.Name -ieq "UNSIGNED_TEST_BUILD"
    })
    if ($markers.Count -gt 0) {
        throw "Signed release candidate contains the UNSIGNED_TEST_BUILD marker."
    }
}

function Get-RequiredReleasePublisherPaths {
    return @(
        "vocekit.exe",
        "ocr/rapidocr/vocekit-rapidocr.exe",
        "ocr/windows/vocekit-windows-ocr.exe",
        "speech/windows/vocekit-windows-speech.exe",
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll"
    )
}

function Get-ReleaseRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $baseFull = [IO.Path]::GetFullPath($BasePath).TrimEnd("\", "/") + "\"
    $pathFull = [IO.Path]::GetFullPath($Path)
    if (-not $pathFull.StartsWith($baseFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the extracted release runtime: $pathFull"
    }
    return $pathFull.Substring($baseFull.Length).Replace("\", "/")
}

function Assert-ExtractedReleaseAuthenticode {
    param(
        [Parameter(Mandatory = $true)][string]$RuntimeDirectory,
        [Parameter(Mandatory = $true)]$CandidateSigner,
        [Parameter(Mandatory = $true)][string]$ExpectedSignerSubject,
        [string]$ExpectedSignerThumbprint = ""
    )

    $runtimeFull = [IO.Path]::GetFullPath($RuntimeDirectory)
    if (-not (Test-Path -LiteralPath $runtimeFull -PathType Container)) {
        throw "Authenticode verification failed: extracted runtime is missing."
    }
    $candidateSubject = [string](Get-RequiredJsonProperty -Object $CandidateSigner -Name "subject" -Context "Candidate signer")
    $candidateThumbprint = ([string](Get-RequiredJsonProperty -Object $CandidateSigner -Name "thumbprint" -Context "Candidate signer") -replace '\s', '').ToUpperInvariant()
    $candidateTimestampSubject = [string](Get-RequiredJsonProperty -Object $CandidateSigner -Name "timestamp_subject" -Context "Candidate signer")
    $candidateTimestampThumbprint = ([string](Get-RequiredJsonProperty -Object $CandidateSigner -Name "timestamp_thumbprint" -Context "Candidate signer") -replace '\s', '').ToUpperInvariant()
    $normalizedExpectedThumbprint = ($ExpectedSignerThumbprint -replace '\s', '').ToUpperInvariant()

    $requiredPublisherPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    Get-RequiredReleasePublisherPaths | ForEach-Object { [void]$requiredPublisherPaths.Add($_) }
    $foundPublisherPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $failures = New-Object Collections.Generic.List[string]
    $binaries = @(Get-ChildItem -LiteralPath $runtimeFull -Recurse -File | Where-Object {
        $_.Extension.ToLowerInvariant() -in @(".exe", ".dll")
    } | Sort-Object FullName)
    if ($binaries.Count -eq 0) {
        $failures.Add("No executable binaries were found in the extracted runtime.")
    }

    foreach ($binary in $binaries) {
        $relativePath = Get-ReleaseRelativePath -BasePath $runtimeFull -Path $binary.FullName
        try {
            $signature = Get-AuthenticodeSignature -LiteralPath $binary.FullName
        } catch {
            $failures.Add("$relativePath`: signature inspection failed: $($_.Exception.Message)")
            continue
        }
        if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
            $failures.Add("$relativePath`: $($signature.Status)")
            continue
        }
        if (-not $requiredPublisherPaths.Contains($relativePath)) {
            continue
        }

        [void]$foundPublisherPaths.Add($relativePath)
        if (-not $signature.SignerCertificate) {
            $failures.Add("$relativePath`: publisher certificate is missing.")
            continue
        }
        $actualSubject = $signature.SignerCertificate.Subject
        $actualThumbprint = ($signature.SignerCertificate.Thumbprint -replace '\s', '').ToUpperInvariant()
        if (-not [string]::Equals($actualSubject, $ExpectedSignerSubject, [StringComparison]::OrdinalIgnoreCase) -or
            -not [string]::Equals($actualSubject, $candidateSubject, [StringComparison]::OrdinalIgnoreCase)) {
            $failures.Add("$relativePath`: publisher subject mismatch.")
        }
        if ($actualThumbprint -cne $candidateThumbprint -or
            (-not [string]::IsNullOrWhiteSpace($normalizedExpectedThumbprint) -and
                $actualThumbprint -cne $normalizedExpectedThumbprint)) {
            $failures.Add("$relativePath`: publisher thumbprint mismatch.")
        }
        if (-not $signature.TimeStamperCertificate) {
            $failures.Add("$relativePath`: RFC 3161 timestamp is missing.")
        } elseif ($relativePath -ieq "vocekit.exe") {
            $timestampThumbprint = ($signature.TimeStamperCertificate.Thumbprint -replace '\s', '').ToUpperInvariant()
            if (-not [string]::Equals(
                    $signature.TimeStamperCertificate.Subject,
                    $candidateTimestampSubject,
                    [StringComparison]::OrdinalIgnoreCase
                ) -or
                $timestampThumbprint -cne $candidateTimestampThumbprint) {
                $failures.Add("vocekit.exe: timestamp identity does not match the candidate record.")
            }
        }
    }
    foreach ($requiredPath in $requiredPublisherPaths) {
        if (-not $foundPublisherPaths.Contains($requiredPath)) {
            $failures.Add("Required publisher-owned binary was not verified: $requiredPath")
        }
    }
    if ($failures.Count -gt 0) {
        throw "Authenticode verification failed:`n- $($failures -join "`n- ")"
    }
}

function Assert-ExtractedReleaseRuntime {
    param(
        [Parameter(Mandatory = $true)][string]$RuntimeDirectory,
        [Parameter(Mandatory = $true)]$Candidate,
        [Parameter(Mandatory = $true)][string]$ExpectedSignerSubject,
        [string]$ExpectedSignerThumbprint = "",
        [Parameter(Mandatory = $true)][string]$ExpectedUpdateFeedUrl,
        [Parameter(Mandatory = $true)][string]$PackageScript,
        [Parameter(Mandatory = $true)][string]$HelperProvenanceVerifier,
        [Parameter(Mandatory = $true)][string]$ProvenanceVerifier,
        [Parameter(Mandatory = $true)][string]$RuntimeVerifier
    )

    # The extracted ZIP is untrusted until every EXE/DLL has a valid signature.
    # Keep this gate before every helper/main provenance probe and runtime test:
    # those checks launch binaries from the extracted directory.
    Assert-ExtractedReleaseAuthenticode `
        -RuntimeDirectory $RuntimeDirectory `
        -CandidateSigner $Candidate.signer `
        -ExpectedSignerSubject $ExpectedSignerSubject `
        -ExpectedSignerThumbprint $ExpectedSignerThumbprint
    Assert-NoUnsignedTestMarker -RuntimeDirectory $RuntimeDirectory
    & $PackageScript -ValidationOnly privacy -ValidationPath $RuntimeDirectory
    Assert-ExtractedRuntimeHelperBindings `
        -RuntimeDirectory $RuntimeDirectory `
        -Bindings @($Candidate.runtime_helpers) `
        -ExpectedSourceCommit ([string]$Candidate.source_commit) `
        -VerifierPath $HelperProvenanceVerifier
    [void](& $ProvenanceVerifier `
        -ExecutablePath (Join-Path $RuntimeDirectory "vocekit.exe") `
        -ExpectedSourceCommit ([string]$Candidate.source_commit) `
        -ExpectedVersion ([string]$Candidate.version) `
        -ExpectedUpdateFeedUrl $ExpectedUpdateFeedUrl `
        -ExpectedConfiguration release)
    & $RuntimeVerifier -Configuration release -RuntimeDir $RuntimeDirectory
}

function Get-ImmutableReleaseFileSnapshots {
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $Paths) {
        $fullPath = [IO.Path]::GetFullPath($path)
        [void](Assert-NoReparsePointsInExistingPathChain `
            -Path $fullPath `
            -Label "Immutable finalization input")
        if (-not $seen.Add($fullPath)) {
            throw "Immutable input path was supplied more than once: $fullPath"
        }
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            throw "Immutable finalization input is missing: $fullPath"
        }
        $item = Get-Item -LiteralPath $fullPath
        [PSCustomObject]@{
            path = $fullPath
            bytes = [long]$item.Length
            sha256 = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToUpperInvariant()
            last_write_utc_ticks = [long]$item.LastWriteTimeUtc.Ticks
        }
    }
}

function Assert-ImmutableReleasePathSetsEqual {
    param(
        [Parameter(Mandatory = $true)][string[]]$Expected,
        [Parameter(Mandatory = $true)][string[]]$Actual
    )

    $expectedPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $Expected) {
        $fullPath = [IO.Path]::GetFullPath($path)
        if (-not $expectedPaths.Add($fullPath)) {
            throw "Immutable release path set contains a duplicate expected path: $fullPath"
        }
    }
    $actualPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $Actual) {
        $fullPath = [IO.Path]::GetFullPath($path)
        if (-not $actualPaths.Add($fullPath)) {
            throw "Immutable release path set contains a duplicate actual path: $fullPath"
        }
    }
    if ($expectedPaths.Count -ne $actualPaths.Count) {
        throw "Immutable release path set changed between confirmation and lock acquisition."
    }
    foreach ($path in $expectedPaths) {
        if (-not $actualPaths.Contains($path)) {
            throw "Immutable release path set changed between confirmation and lock acquisition: $path"
        }
    }
}

function Open-ImmutableReleaseFileLocks {
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    $locks = New-Object Collections.Generic.List[IO.FileStream]
    try {
        foreach ($path in $Paths) {
            $fullPath = [IO.Path]::GetFullPath($path)
            [void](Assert-NoReparsePointsInExistingPathChain `
                -Path $fullPath `
                -Label "Immutable finalization input")
            $locks.Add([IO.File]::Open(
                $fullPath,
                [IO.FileMode]::Open,
                [IO.FileAccess]::Read,
                [IO.FileShare]::Read
            ))
        }
        return @($locks | ForEach-Object { $_ })
    } catch {
        foreach ($lock in $locks) {
            $lock.Dispose()
        }
        throw
    }
}

function Close-ImmutableReleaseFileLocks {
    param([object[]]$Locks)

    foreach ($lock in @($Locks)) {
        if ($null -ne $lock) {
            $lock.Dispose()
        }
    }
}

function Assert-ImmutableReleaseFileSnapshotsEqual {
    param(
        [Parameter(Mandatory = $true)][object[]]$Before,
        [Parameter(Mandatory = $true)][object[]]$After
    )

    if ($Before.Count -ne $After.Count) {
        throw "Immutable release inputs changed during finalization."
    }
    $afterByPath = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($snapshot in $After) {
        $path = [string]$snapshot.path
        if ($afterByPath.ContainsKey($path)) {
            throw "Immutable release input snapshot contains a duplicate path: $path"
        }
        $afterByPath.Add($path, $snapshot)
    }
    foreach ($beforeSnapshot in $Before) {
        $path = [string]$beforeSnapshot.path
        if (-not $afterByPath.ContainsKey($path)) {
            throw "Immutable release input disappeared during finalization: $path"
        }
        $afterSnapshot = $afterByPath[$path]
        if ([long]$beforeSnapshot.bytes -ne [long]$afterSnapshot.bytes -or
            [string]$beforeSnapshot.sha256 -cne [string]$afterSnapshot.sha256 -or
            [long]$beforeSnapshot.last_write_utc_ticks -ne [long]$afterSnapshot.last_write_utc_ticks) {
            throw "Immutable release input changed during finalization: $path"
        }
    }
}

function Assert-ReleaseTagMessageBindings {
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [Parameter(Mandatory = $true)][string]$Tag,
        [Parameter(Mandatory = $true)][string]$SourceCommit,
        [Parameter(Mandatory = $true)][string]$ArchiveSha256,
        [Parameter(Mandatory = $true)][string]$EvidenceSha256
    )

    if ($Message -notmatch "(?m)^VoceKit release $([regex]::Escape($Tag))\s*$") {
        throw "Annotated tag message is missing the exact release heading."
    }
    foreach ($binding in @(
        @{ Name = "source-commit"; Value = $SourceCommit; Pattern = '[0-9a-fA-F]{40}(?:[0-9a-fA-F]{24})?' },
        @{ Name = "archive-sha256"; Value = $ArchiveSha256; Pattern = '[0-9a-fA-F]{64}' },
        @{ Name = "evidence-sha256"; Value = $EvidenceSha256; Pattern = '[0-9a-fA-F]{64}' }
    )) {
        $matches = [regex]::Matches(
            $Message,
            "(?m)^$([regex]::Escape($binding.Name)):\s*($($binding.Pattern))\s*$"
        )
        if ($matches.Count -ne 1 -or
            $matches[0].Groups[1].Value.ToUpperInvariant() -cne
                ([string]$binding.Value).ToUpperInvariant()) {
            throw "Annotated tag message does not exactly bind $($binding.Name)."
        }
    }
}

function Invoke-ReleaseGit {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string[]]$GitArguments
    )

    $result = Invoke-TrustedGit `
        -RepositoryRoot $RepositoryRoot `
        -Arguments $GitArguments
    return @($result.output)
}

function Assert-ReleaseFinalizationGitState {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)]$Candidate,
        [Parameter(Mandatory = $true)][string]$EvidencePath
    )

    $repositoryFull = [IO.Path]::GetFullPath($RepositoryRoot)
    $projectFull = [IO.Path]::GetFullPath($ProjectRoot)
    $versionPath = Join-Path $projectFull "APP_VERSION"
    if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) {
        throw "APP_VERSION is missing during release finalization."
    }
    $version = (Get-Content -LiteralPath $versionPath -Raw -Encoding UTF8).Trim()
    if ($version -cne [string]$Candidate.version -or
        [string]$Candidate.tag -cne "v$version") {
        throw "APP_VERSION and the candidate version/tag do not match."
    }

    Assert-NoHiddenGitIndexEntries -RepositoryRoot $repositoryFull
    $status = @(Invoke-ReleaseGit -RepositoryRoot $repositoryFull -GitArguments @(
        "status", "--porcelain", "--untracked-files=all"
    ))
    if ($status.Count -gt 0) {
        throw "Git worktree is not clean during final release approval."
    }
    [void](Invoke-ReleaseGit -RepositoryRoot $repositoryFull -GitArguments @(
        "fetch", "--quiet", "origin", "main"
    ))
    $head = (@(Invoke-ReleaseGit -RepositoryRoot $repositoryFull -GitArguments @("rev-parse", "HEAD")) -join "").Trim()
    $originMain = (@(Invoke-ReleaseGit -RepositoryRoot $repositoryFull -GitArguments @("rev-parse", "origin/main")) -join "").Trim()
    if ($head -cne [string]$Candidate.source_commit -or
        $originMain -cne [string]$Candidate.source_commit) {
        throw "Candidate source_commit must exactly match HEAD and origin/main."
    }

    $tagRef = "refs/tags/$($Candidate.tag)"
    $tagType = (@(Invoke-ReleaseGit -RepositoryRoot $repositoryFull -GitArguments @("cat-file", "-t", $tagRef)) -join "").Trim()
    if ($tagType -cne "tag") {
        throw "Final release tag must exist locally as an annotated tag: $($Candidate.tag)"
    }
    $tagCommit = (@(Invoke-ReleaseGit -RepositoryRoot $repositoryFull -GitArguments @(
        "rev-parse", "$tagRef^{commit}"
    )) -join "").Trim()
    if ($tagCommit -cne [string]$Candidate.source_commit) {
        throw "Annotated release tag does not point to candidate source_commit."
    }
    $tagMessage = @(Invoke-ReleaseGit -RepositoryRoot $repositoryFull -GitArguments @(
        "for-each-ref", "--format=%(contents)", $tagRef
    )) -join "`n"
    Assert-ReleaseTagMessageBindings `
        -Message $tagMessage `
        -Tag ([string]$Candidate.tag) `
        -SourceCommit ([string]$Candidate.source_commit) `
        -ArchiveSha256 ([string]$Candidate.archive.sha256) `
        -EvidenceSha256 ((Get-FileHash -LiteralPath $EvidencePath -Algorithm SHA256).Hash)
}

if ($DecisionTestMode) {
    return
}

$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ".."))
$candidateControlBefore = @(Get-ImmutableReleaseFileSnapshots -Paths @($CandidatePath, $EvidencePath))
$verified = Confirm-ReleaseCandidateMetadata `
    -CandidatePath $CandidatePath `
    -EvidencePath $EvidencePath `
    -ExpectedSignerSubject $ExpectedSignerSubject `
    -ExpectedSignerThumbprint $ExpectedSignerThumbprint `
    -ExpectedUpdateFeedUrl $ExpectedUpdateFeedUrl `
    -ExpectedReleaseBaseUrl $ExpectedReleaseBaseUrl `
    -ExpectedReleasePageBaseUrl $ExpectedReleasePageBaseUrl `
    -ProjectRoot $projectRoot
$candidateControlAfter = @(Get-ImmutableReleaseFileSnapshots -Paths @($verified.CandidatePath, $verified.EvidencePath))
Assert-ImmutableReleaseFileSnapshotsEqual `
    -Before $candidateControlBefore `
    -After $candidateControlAfter

$immutablePaths = @(
    $verified.CandidatePath,
    $verified.EvidencePath,
    $verified.ArchivePath,
    $verified.SidecarPath,
    $verified.ManifestPath
) + @($verified.EvidenceAttachmentPaths)
$extractionRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ("vocekit-release-finalization-" + [Guid]::NewGuid().ToString("N"))
$packageScript = Join-Path $PSScriptRoot "package-test.ps1"
$runtimeVerifier = Join-Path $PSScriptRoot "verify-runtime.ps1"
$provenanceVerifier = Join-Path $PSScriptRoot "verify-embedded-build-provenance.ps1"
$helperProvenanceVerifier = Join-Path $PSScriptRoot "verify-runtime-helper-build-provenance.ps1"
$locks = @()
$snapshotBefore = $null
try {
    # FileShare.Read denies writes, deletes, and atomic replacements while the
    # complete second confirmation and runtime inspection are in progress.
    $locks = @(Open-ImmutableReleaseFileLocks -Paths $immutablePaths)
    $snapshotBefore = @(Get-ImmutableReleaseFileSnapshots -Paths $immutablePaths)
    $lockedCandidateControl = @(Get-ImmutableReleaseFileSnapshots -Paths @(
        $verified.CandidatePath,
        $verified.EvidencePath
    ))
    Assert-ImmutableReleaseFileSnapshotsEqual `
        -Before $candidateControlBefore `
        -After $lockedCandidateControl
    $verified = Confirm-ReleaseCandidateMetadata `
        -CandidatePath $CandidatePath `
        -EvidencePath $EvidencePath `
        -ExpectedSignerSubject $ExpectedSignerSubject `
        -ExpectedSignerThumbprint $ExpectedSignerThumbprint `
        -ExpectedUpdateFeedUrl $ExpectedUpdateFeedUrl `
        -ExpectedReleaseBaseUrl $ExpectedReleaseBaseUrl `
        -ExpectedReleasePageBaseUrl $ExpectedReleasePageBaseUrl `
        -ProjectRoot $projectRoot
    $lockedVerifiedPaths = @(
        $verified.CandidatePath,
        $verified.EvidencePath,
        $verified.ArchivePath,
        $verified.SidecarPath,
        $verified.ManifestPath
    ) + @($verified.EvidenceAttachmentPaths)
    Assert-ImmutableReleasePathSetsEqual `
        -Expected $immutablePaths `
        -Actual $lockedVerifiedPaths
    $snapshotAfterLockedConfirmation = @(Get-ImmutableReleaseFileSnapshots -Paths $immutablePaths)
    Assert-ImmutableReleaseFileSnapshotsEqual `
        -Before $snapshotBefore `
        -After $snapshotAfterLockedConfirmation
    Assert-ReleaseFinalizationGitState `
        -RepositoryRoot $RepositoryRoot `
        -ProjectRoot $projectRoot `
        -Candidate $verified.Candidate `
        -EvidencePath $verified.EvidencePath

    try {
        Expand-VerifiedReleaseArchive `
            -ArchivePath $verified.ArchivePath `
            -DestinationPath $extractionRoot
        Assert-ExtractedReleaseRuntime `
            -RuntimeDirectory $extractionRoot `
            -Candidate $verified.Candidate `
            -ExpectedSignerSubject $ExpectedSignerSubject `
            -ExpectedUpdateFeedUrl $ExpectedUpdateFeedUrl `
            -ExpectedSignerThumbprint $ExpectedSignerThumbprint `
            -PackageScript $packageScript `
            -HelperProvenanceVerifier $helperProvenanceVerifier `
            -ProvenanceVerifier $provenanceVerifier `
            -RuntimeVerifier $runtimeVerifier
    } finally {
        if ([IO.Directory]::Exists($extractionRoot)) {
            [IO.Directory]::Delete($extractionRoot, $true)
        }
    }
} finally {
    try {
        if ($null -ne $snapshotBefore) {
            $snapshotAfter = @(Get-ImmutableReleaseFileSnapshots -Paths $immutablePaths)
            Assert-ImmutableReleaseFileSnapshotsEqual -Before $snapshotBefore -After $snapshotAfter
        }
    } finally {
        Close-ImmutableReleaseFileLocks -Locks $locks
    }
}

Write-Host "Signed release candidate finalization passed without modifying candidate or evidence bytes."
Write-Host "  Version: $($verified.Candidate.version)"
Write-Host "  Tag: $($verified.Candidate.tag)"
Write-Host "  Source commit: $($verified.Candidate.source_commit)"
Write-Host "  Archive SHA-256: $($verified.Candidate.archive.sha256)"
