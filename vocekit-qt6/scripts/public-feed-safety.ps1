Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-PublicFeedRequestFailureSummary {
    param([Parameter(Mandatory = $true)]$ErrorRecord)

    $statusCode = $null
    try {
        if ($null -ne $ErrorRecord.Exception.Response -and
            $null -ne $ErrorRecord.Exception.Response.StatusCode) {
            $statusCode = [int]$ErrorRecord.Exception.Response.StatusCode
        }
    } catch {
        $statusCode = $null
    }

    $message = ([string]$ErrorRecord.Exception.Message -replace '\s+', ' ').Trim()
    if ($message.Length -gt 240) {
        $message = $message.Substring(0, 240) + "..."
    }
    if ($null -ne $statusCode) {
        return "HTTP $statusCode ($message)"
    }
    if ([string]::IsNullOrWhiteSpace($message)) {
        return $ErrorRecord.Exception.GetType().FullName
    }
    return $message
}

function Test-PublicReleaseFeedReachability {
    param(
        [Parameter(Mandatory = $true)][Uri]$FeedUri,
        [Parameter(Mandatory = $true)][string]$UserAgent,
        [scriptblock]$RequestAction = {
            param([Uri]$Uri, [hashtable]$Headers)
            Invoke-WebRequest `
                -Uri $Uri `
                -Headers $Headers `
                -Method Get `
                -TimeoutSec 20 `
                -UseBasicParsing
        }
    )

    $jsonHeaders = @{
        "User-Agent" = $UserAgent
        "Accept" = "application/vnd.github+json, application/json"
    }
    try {
        [void](& $RequestAction $FeedUri $jsonHeaders)
        return [PSCustomObject]@{
            is_reachable = $true
            evidence = "update-feed"
            warning = ""
            failure = ""
        }
    } catch {
        $feedFailure = Get-PublicFeedRequestFailureSummary -ErrorRecord $_
    }

    $githubMatch = [regex]::Match(
        $FeedUri.AbsoluteUri,
        '^https://api\.github\.com/repos/([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)/releases/latest/?$'
    )
    if (-not $githubMatch.Success) {
        return [PSCustomObject]@{
            is_reachable = $false
            evidence = ""
            warning = ""
            failure = "The configured update feed is not publicly reachable: $feedFailure"
        }
    }

    $repositorySlug = $githubMatch.Groups[1].Value
    $repositoryApi = [Uri]"https://api.github.com/repos/$repositorySlug"
    try {
        [void](& $RequestAction $repositoryApi $jsonHeaders)
        return [PSCustomObject]@{
            is_reachable = $true
            evidence = "github-repository-api"
            warning = "The public GitHub repository is reachable, but the latest release feed is unavailable ($feedFailure)."
            failure = ""
        }
    } catch {
        $repositoryApiFailure = Get-PublicFeedRequestFailureSummary -ErrorRecord $_
    }

    # Shared GitHub-hosted runner addresses can exhaust the anonymous REST API
    # quota. The public repository HTML page is an independent anonymous check:
    # private or missing repositories fail closed, while a public page remains
    # verifiable without weakening the release gate or using a repository token.
    $repositoryPage = [Uri]"https://github.com/$repositorySlug"
    $htmlHeaders = @{
        "User-Agent" = $UserAgent
        "Accept" = "text/html,application/xhtml+xml"
    }
    try {
        [void](& $RequestAction $repositoryPage $htmlHeaders)
        return [PSCustomObject]@{
            is_reachable = $true
            evidence = "github-public-page"
            warning = "The public GitHub repository page is reachable, but the latest release feed ($feedFailure) and anonymous repository API check ($repositoryApiFailure) were unavailable."
            failure = ""
        }
    } catch {
        $repositoryPageFailure = Get-PublicFeedRequestFailureSummary -ErrorRecord $_
    }

    return [PSCustomObject]@{
        is_reachable = $false
        evidence = ""
        warning = ""
        failure = "The configured GitHub update repository is not publicly reachable (feed: $feedFailure; repository API: $repositoryApiFailure; public page: $repositoryPageFailure)."
    }
}
