param(
    [string]$PythonPath,
    [string]$SaveKey = 'F6',
    [string]$RestoreKey = 'F7'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $PythonPath) {
    $bundledPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $bundledPython) { $PythonPath = $bundledPython }
    else { $PythonPath = (Get-Command python -ErrorAction Stop).Source }
}
$game = @(Get-Process | Where-Object { $_.ProcessName -eq 'Le Mans Ultimate' })
if ($game.Count -ne 1) { throw 'Open one LMU instance in offline Practice without EAC.' }
Write-Host "$SaveKey saves; $RestoreKey restores. Release both keys before using them."
Push-Location $projectRoot
try {
    & $PythonPath scripts/sector.py runtime.start_reset_hotkeys --pid $game[0].Id --save-key $SaveKey --restore-key $RestoreKey --offline-launch-confirmed --allow-writes
    if ($LASTEXITCODE -ne 0) { throw 'Connection refused; see the reason above.' }
}
finally { Pop-Location }
