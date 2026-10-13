param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$fuelWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$fuelSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $fuelWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $fuelWork 'zig-local-cache'
$fuelCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu',"$fuelSrc\rigid_restore_core.c","$fuelSrc\fuel_restore_core.c")
& $ZigPath @fuelCommon "$fuelSrc\fuel_restore_fixture.c" -o "$fuelWork\fuel-restore-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Fuel fixture build failed'}
& "$fuelWork\fuel-restore-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Fuel fixture failed'}
& $ZigPath @fuelCommon -shared "$fuelSrc\fuel_fixture_bridge.c" -o "$fuelWork\fuel-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Fuel local replay bridge failed'}
