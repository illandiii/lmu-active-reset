param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')

$ErrorActionPreference='Stop'
$runtimeWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $runtimeWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $runtimeWork 'zig-local-cache'
& $ZigPath cc -O2 -Wall -Wextra -Werror (Get-SectorNativeSource 'active_reset_control.c') (Get-SectorNativeSource 'active_reset_control_fixture.c') -o (Join-Path $runtimeWork 'active-reset-control-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Control build failed'}
& (Join-Path $runtimeWork 'active-reset-control-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Control fixture failed'}
$runtimeSources=@('active_reset_lab.cpp','active_reset_hotkeys_win32.cpp','active_reset_native.c','active_reset_control.c','restore_flow.c','native_memory_backend.c','physical_scoring_transaction.c','restore_transaction.c','vehicle_memory_bindings.c','lap_scoring_plan.c') | ForEach-Object {Get-SectorNativeSource $_}
& $ZigPath c++ -O2 -Wall -Wextra -Werror -Wno-deprecated -target x86_64-windows-gnu @runtimeSources -luser32 -lgdi32 -o (Join-Path $projectRoot 'bin\active-reset-lab.exe')
if($LASTEXITCODE -ne 0){throw 'Offline runtime build failed'}
& (Join-Path $projectRoot 'bin\active-reset-lab.exe') --self-test
if($LASTEXITCODE -ne 0){throw 'Offline runtime self-test failed'}
