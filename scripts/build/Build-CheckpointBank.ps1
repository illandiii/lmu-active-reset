param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')

$ErrorActionPreference='Stop'
$bankWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $bankWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $bankWork 'zig-local-cache'
$bankSources=@('rigid_restore_core','bar_constraint_core','ball_constraint_core','angular_constraint_core','constraint_group_core','fuel_restore_core','wheel_inertia_core','tire_inertia_core','mechanical_prepare_core','checkpoint_bank','tire_persistent_core','checkpoint_persistent_bank','checkpoint_vehicle_partial','checkpoint_transmission_partial','checkpoint_cached_mass','checkpoint_solver_history','checkpoint_engine_integration','restore_transaction','restore_plan_25661166','checkpoint_native_cache','restore_bindings_25661166','checkpoint_tire_thermal','checkpoint_tread_history','wheel_restore_core','checkpoint_wheel_phase','restore_thermal_plan','checkpoint_brake_state','checkpoint_driveline_cache','checkpoint_driveline_outputs','checkpoint_powertrain_control','checkpoint_shift_transient','restore_joint_25661166','restore_flow','scoring_plan_25661166') | ForEach-Object {Get-SectorNativeSource ($_.ToString()+'.c')}
& $ZigPath cc -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu @bankSources (Get-SectorNativeSource 'checkpoint_bank_fixture.c') -o "$bankWork\checkpoint-bank-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Checkpoint bank build failed'}
& "$bankWork\checkpoint-bank-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Checkpoint bank fixture failed'}
& $ZigPath cc -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu -shared @bankSources (Get-SectorNativeSource 'restore_native_memory_25661166.c') (Get-SectorNativeSource 'checkpoint_bank_bridge.c') -o "$bankWork\checkpoint-bank-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Checkpoint bank replay bridge failed'}
