param([string]$GamePath = 'D:\SteamLibrary\steamapps\common\Le Mans Ultimate',
      [string]$OutputPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
if (-not $OutputPath) { $OutputPath = Join-Path $projectRoot 'observed-inventory.json' }
# Inspection never replaces the pinned compatibility inventory used by Capture.ps1.
$relativePaths = @('Le Mans Ultimate.exe','PluginsAdapter.exe',
    'Support\SharedMemoryInterface\SharedMemoryInterface.hpp',
    'Support\SharedMemoryInterface\InternalsPlugin.hpp',
    'Support\SharedMemoryInterface\PluginObjects.hpp')
$records = foreach ($relativePath in $relativePaths) {
    $item = Get-Item -LiteralPath (Join-Path $GamePath $relativePath)
    [pscustomobject]@{relative_path=$relativePath;size=$item.Length;
        sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash;
        file_version=$item.VersionInfo.FileVersion}
}
$manifest = Join-Path $GamePath '..\..\appmanifest_2399420.acf'
$build = $null
if (Test-Path -LiteralPath $manifest) {
    $line = Select-String -LiteralPath $manifest -Pattern '^\s*"buildid"\s*"(\d+)"'
    if ($line) { $build = $line.Matches[0].Groups[1].Value }
}
[pscustomobject]@{recorded_utc=[DateTime]::UtcNow.ToString('o');install_path=$GamePath;
    steam_appid=2399420;steam_buildid=$build;files=$records} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $OutputPath
Write-Output $OutputPath
