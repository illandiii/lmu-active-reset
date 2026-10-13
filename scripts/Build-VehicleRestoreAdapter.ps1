param(
    [Parameter(Mandatory = $true)][string]$ZigPath,
    [string]$FrameDirectory
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $projectRoot 'src'
$buildRoot = Join-Path $projectRoot 'work'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $buildRoot 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $buildRoot 'zig-local-cache'
$core = @(
    'rigid_restore_core', 'bar_constraint_core', 'ball_constraint_core',
    'angular_constraint_core', 'constraint_group_core', 'fuel_restore_core',
    'wheel_inertia_core', 'tire_inertia_core', 'mechanical_prepare_core',
    'checkpoint_bank', 'tire_persistent_core', 'checkpoint_persistent_bank',
    'checkpoint_vehicle_partial', 'checkpoint_transmission_partial',
    'checkpoint_cached_mass', 'checkpoint_solver_history', 'checkpoint_engine_integration',
    'restore_transaction', 'restore_plan_25661166', 'checkpoint_native_cache',
    'restore_bindings_25661166', 'checkpoint_tire_thermal', 'checkpoint_tread_history',
    'wheel_restore_core', 'checkpoint_wheel_phase', 'restore_thermal_plan',
    'checkpoint_brake_state', 'checkpoint_driveline_cache', 'checkpoint_driveline_outputs',
    'checkpoint_powertrain_control', 'checkpoint_shift_transient', 'restore_joint_25661166',
    'restore_flow', 'scoring_plan_25661166', 'restore_native_memory_25661166',
    'active_reset_control', 'active_reset_native'
) | ForEach-Object { Join-Path $sourceRoot "${_}.c" }
$native = @(
    'vehicle_checkpoint_25661166', 'vehicle_reader_25661166',
    'vehicle_restore_adapter_25661166', 'contact_batch_25661166',
    'native_lock_witness', 'native_lifetime_pin', 'native_lifetime_handoff',
    'fenced_restore', 'simulation_fence', 'active_reset_boundary'
) | ForEach-Object { Join-Path $sourceRoot "${_}.cpp" }
$options = @('-O2', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated', '-target', 'x86_64-windows-gnu')

# Compile the production translation units separately; no hook or injection entry.
& $ZigPath c++ @options -shared @core @native -o (Join-Path $buildRoot 'vehicle-restore-adapter.dll')
if ($LASTEXITCODE -ne 0) { throw 'Production adapter build failed' }

$fixture = Join-Path $buildRoot 'vehicle-restore-adapter-fixture.exe'
& $ZigPath c++ @options -DLMU_VEHICLE_ADAPTER_FIXTURE @core @native `
    (Join-Path $sourceRoot 'vehicle_restore_adapter_fixture.cpp') -o $fixture
if ($LASTEXITCODE -ne 0) { throw 'Vehicle adapter fixture build failed' }

if ($FrameDirectory) {
    foreach ($faultCase in 0..2) {
        & $fixture (Join-Path $FrameDirectory 'saved.frame') (Join-Path $FrameDirectory 'current.frame') $faultCase
        if ($LASTEXITCODE -ne 0) { throw "Vehicle adapter fixture failed: $faultCase" }
    }
}
