Set-StrictMode -Version Latest

function Assert-NoReparsePointsInExistingPathChain {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string]$Label = "Release path"
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "$Label must not be blank."
    }
    $fullPath = [IO.Path]::GetFullPath($Path)
    $root = [IO.Path]::GetPathRoot($fullPath)
    if ([string]::IsNullOrWhiteSpace($root)) {
        throw "$Label does not have a filesystem root: $fullPath"
    }

    $current = $root
    if (Test-Path -LiteralPath $current) {
        $rootItem = Get-Item -LiteralPath $current -Force -ErrorAction Stop
        if (($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Label traverses a reparse point: $current"
        }
    }

    $relative = $fullPath.Substring($root.Length)
    $segments = @($relative.Split(
        [char[]]@('\', '/'),
        [StringSplitOptions]::RemoveEmptyEntries
    ))
    foreach ($segment in $segments) {
        if ($segment.Contains(":")) {
            throw "$Label contains an alternate data stream or unsafe path segment: $fullPath"
        }
        $current = Join-Path $current $segment
        if (-not (Test-Path -LiteralPath $current)) {
            break
        }
        $item = Get-Item -LiteralPath $current -Force -ErrorAction Stop
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Label traverses a reparse point: $current"
        }
    }

    return $fullPath
}
