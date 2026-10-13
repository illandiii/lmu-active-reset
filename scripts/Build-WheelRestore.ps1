param([string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$wheelWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
if (-not $ZigPath) { $ZigPath=Join-Path $wheelWork 'tools\zig-x86_64-windows-0.16.0\zig.exe' }
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $wheelWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $wheelWork 'zig-local-cache'
$wheelSrc=Join-Path $projectRoot 'src'
$wheelCommon=@('cc','-O2','-target','x86_64-windows-gnu',"$wheelSrc\rigid_restore_core.c","$wheelSrc\wheel_restore_core.c")
& $ZigPath @wheelCommon "$wheelSrc\wheel_restore_fixture.c" -o "$wheelWork\wheel-restore-fixture.exe" 2> "$wheelWork\wheel-build.log"
if($LASTEXITCODE -ne 0){throw 'Wheel fixture build failed; inspect work/wheel-build.log'}
& "$wheelWork\wheel-restore-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Wheel analytical fixture failed'}
& $ZigPath @wheelCommon -shared "$wheelSrc\wheel_fixture_bridge.c" -o "$wheelWork\wheel-fixture.dll" 2> "$wheelWork\wheel-bridge-build.log"
if($LASTEXITCODE -ne 0){throw 'Local wheel bridge build failed'}
