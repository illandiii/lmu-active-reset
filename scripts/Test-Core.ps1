param([Parameter(Mandatory = $true)][string]$ZigPath)

$ErrorActionPreference = 'Stop'
$builds = @(
    'Build-RestoreTransaction.ps1',
    'Build-RestoreFlow.ps1',
    'Build-NativeMemory.ps1',
    'Build-NativeLifetimePin.ps1',
    'Build-NativeLifetimeHandoff.ps1',
    'Build-ContactBatch.ps1',
    'Build-CheckpointBank.ps1',
    'Build-ActiveResetRuntime.ps1',
    'Build-ActiveResetBoundary.ps1',
    'Build-SimulationFence.ps1'
)

foreach ($build in $builds) {
    Write-Host "Running $build"
    & (Join-Path $PSScriptRoot $build) -ZigPath $ZigPath
    if ($LASTEXITCODE -ne 0) { throw "$build failed" }
}
