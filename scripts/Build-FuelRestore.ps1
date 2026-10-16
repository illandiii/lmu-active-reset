param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourcePaths.ps1')

$ErrorActionPreference='Stop'
$fuelWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $fuelWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $fuelWork 'zig-local-cache'
$fuelCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu',(Get-SectorNativeSource 'rigid_restore_core.c'),(Get-SectorNativeSource 'fuel_restore_core.c'))
& $ZigPath @fuelCommon (Get-SectorNativeSource 'fuel_restore_fixture.c') -o "$fuelWork\fuel-restore-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Fuel fixture build failed'}
& "$fuelWork\fuel-restore-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Fuel fixture failed'}
& $ZigPath @fuelCommon -shared (Get-SectorNativeSource 'fuel_fixture_bridge.c') -o "$fuelWork\fuel-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Fuel local replay bridge failed'}
