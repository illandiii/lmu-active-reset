param([Alias('IncludeWpf')][switch]$IncludeElectron)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testTemp = Join-Path $projectRoot 'work/test-temp'
[IO.Directory]::CreateDirectory($testTemp) | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
Push-Location (Join-Path $projectRoot 'src/ui/electron')
try {
    $env:TEMP = $testTemp
    $env:TMP = $testTemp
    & npm.cmd test
    if ($LASTEXITCODE -ne 0) { throw 'Electron controller tests failed.' }
    if ($IncludeElectron) {
        & npm.cmd run build
        if ($LASTEXITCODE -ne 0) { throw 'Electron renderer build failed.' }
        & npm.cmd run test:ui
        if ($LASTEXITCODE -ne 0) { throw 'Electron window tests failed.' }
    }
} finally {
    Pop-Location
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
}
