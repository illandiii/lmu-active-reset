param([ValidateRange(1,3600)][int]$Seconds = 30,
      [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Label = 'baseline')
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference = 'Stop'
$inventory = Get-Content -LiteralPath (Join-Path $projectRoot 'inventory.json') -Raw | ConvertFrom-Json
$gamePath = $inventory.install_path
foreach ($entry in $inventory.files) {
    if ((Get-FileHash -LiteralPath (Join-Path $gamePath $entry.relative_path) -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Build changed; capture refused: $($entry.relative_path)"
    }
}
$games = @(Get-Process -Name 'Le Mans Ultimate' -ErrorAction Stop)
if ($games.Count -ne 1) { throw 'Exactly one LMU process required' }
if ($games[0].Path -ne (Join-Path $gamePath 'Le Mans Ultimate.exe')) { throw 'Running executable path differs from inventory' }
$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$log = Join-Path $projectRoot "logs\${stamp}_${Label}.jsonl"
$metadata = [ordered]@{ started_utc=[DateTime]::UtcNow.ToString('o'); label=$Label; duration_requested_s=$Seconds; steam_build=$inventory.steam_buildid; exe_sha256=$inventory.files[0].sha256; diagnostic_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'bin\diagnostic.exe') -Algorithm SHA256).Hash; offline_evidence='not_machine_verified'; purpose='external diagnostic only' }
$metadata | ConvertTo-Json | Set-Content -LiteralPath "$log.metadata.json"
& (Join-Path $projectRoot 'bin\diagnostic.exe') $games[0].Id $Seconds $log
$result = $LASTEXITCODE
Write-Output "Log: $log"
if ($result -ne 0) { throw "Capture failed (exit $result)" }
