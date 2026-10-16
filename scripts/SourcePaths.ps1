# Build lists keep stable basenames; resolve them to the organized source tree.
$script:sectorNativeSources = @{}
$sectorRepository = Split-Path -Parent $PSScriptRoot
foreach ($directory in @('src/native', 'tests/native')) {
    foreach ($sourceFile in (Get-ChildItem -LiteralPath (Join-Path $sectorRepository $directory) -File -Recurse)) {
        if ($script:sectorNativeSources.ContainsKey($sourceFile.Name)) {
            throw "Ambiguous native source name: $($sourceFile.Name)"
        }
        $script:sectorNativeSources[$sourceFile.Name] = $sourceFile.FullName
    }
}

function Get-SectorNativeSource([string]$Name) {
    if (-not $script:sectorNativeSources.ContainsKey($Name)) {
        throw "Native source not found: $Name"
    }
    return $script:sectorNativeSources[$Name]
}
