param(
    [string]$CodeCapture,
    [string]$JavaHome = (Join-Path $env:USERPROFILE '.jdks\openjdk-26.0.1'),
    [string]$OutputDirectory
)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$ErrorActionPreference = 'Stop'
$analysisRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot '.'))
$ghidraDirectory = Join-Path $analysisRoot 'work\tools\ghidra_12.1.4_PUBLIC'
$reaPackage = Join-Path $analysisRoot 'work\rea\node_modules\rea-agents\package.json'
if (!(Test-Path -LiteralPath $reaPackage)) { throw 'Install the pinned REA dependencies in work/rea first.' }
if ((Get-Content -LiteralPath $reaPackage -Raw | ConvertFrom-Json).version -ne '6.4.0') { throw 'Expected REA 6.4.0' }
if (!(Test-Path -LiteralPath (Join-Path $JavaHome 'bin\javac.exe'))) { throw 'A full JDK is required.' }
if (!(Test-Path -LiteralPath (Join-Path $ghidraDirectory 'support\analyzeHeadless.bat'))) { throw 'Ghidra 12.1.4 is missing.' }
if (!$CodeCapture) { $CodeCapture = Join-Path $analysisRoot 'work\rea\LMU-25661166-live-code.analysis.bin' }
if (!$OutputDirectory) { $OutputDirectory = Join-Path $projectRoot ('analysis\rea-25661166\'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')) }
$priorJava = $env:JAVA_HOME
$priorGhidra = $env:GHIDRA_INSTALL_DIR
$priorHeap = $env:GHIDRA_HEADLESS_MAXMEM
$priorTimeout = $env:REA_GHIDRA_STARTUP_TIMEOUT_MS
try {
    $env:JAVA_HOME = $JavaHome
    $env:GHIDRA_INSTALL_DIR = $ghidraDirectory
    $env:GHIDRA_HEADLESS_MAXMEM = '4G'
    $env:REA_GHIDRA_STARTUP_TIMEOUT_MS = '900000'
    & node (Join-Path $projectRoot 'tools/rea/rea_lmu_analysis.mjs') $CodeCapture $OutputDirectory
    if ($LASTEXITCODE -ne 0) { throw "REA analysis failed; inspect $OutputDirectory" }
} finally {
    $env:JAVA_HOME = $priorJava
    $env:GHIDRA_INSTALL_DIR = $priorGhidra
    $env:GHIDRA_HEADLESS_MAXMEM = $priorHeap
    $env:REA_GHIDRA_STARTUP_TIMEOUT_MS = $priorTimeout
}
