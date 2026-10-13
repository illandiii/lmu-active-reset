param(
    [string]$PythonPath,
    [string]$SaveKey = 'F6',
    [string]$RestoreKey = 'F7'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $PythonPath) {
    $bundledPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $bundledPython) { $PythonPath = $bundledPython }
    else { $PythonPath = (Get-Command python -ErrorAction Stop).Source }
}
$game = @(Get-Process | Where-Object { $_.ProcessName -eq 'Le Mans Ultimate' })
if ($game.Count -ne 1) { throw 'Apri una sola istanza di LMU in Practice offline senza EAC e premi GUIDA.' }
Write-Host 'Test sperimentale: nessun limite di distanza o velocita; reset in movimento ancora da validare in LMU.'
Write-Host "$SaveKey salva; $RestoreKey ripristina. Torna su LMU e rilascia entrambi i tasti prima di usarli."
Push-Location $projectRoot
try {
    & $PythonPath src/start_reset_hotkeys.py --pid $game[0].Id --save-key $SaveKey --restore-key $RestoreKey --offline-launch-confirmed --allow-writes
    if ($LASTEXITCODE -ne 0) { throw 'Avvio o test rifiutato: leggi il motivo sopra.' }
}
finally { Pop-Location }
