param([Parameter(Mandatory = $true)][string]$ZigPath)

$ErrorActionPreference = 'Stop'
$builds = @(
    'build/Build-PhysicalResetTrace.ps1',
    'build/Build-ResetHostPolicy.ps1',
    'build/Build-RestoreTransaction.ps1',
    'build/Build-RestoreFlow.ps1',
    'build/Build-NativeMemory.ps1',
    'build/Build-NativeLifetimePin.ps1',
    'build/Build-NativeLifetimeHandoff.ps1',
    'build/Build-NativeLifecycleEpoch.ps1',
    'build/Build-ContactBatch.ps1',
    'build/Build-CheckpointBank.ps1',
    'build/Build-ActiveResetRuntime.ps1',
    'build/Build-ActiveResetBoundary.ps1',
    'build/Build-SimulationFence.ps1'
)

foreach ($build in $builds) {
    Write-Host "Running $build"
    & (Join-Path (Split-Path -Parent $PSScriptRoot) $build) -ZigPath $ZigPath
    if ($LASTEXITCODE -ne 0) { throw "$build failed" }
}
