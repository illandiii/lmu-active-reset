param(
    [Parameter(Mandatory = $true)][string]$ZigPath,
    [string]$FrameDirectory,
    [ValidateRange(1, 99)][int]$Revision = 21
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')
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
    'restore_transaction', 'vehicle_restore_plan', 'checkpoint_native_cache',
    'vehicle_memory_bindings', 'checkpoint_tire_thermal', 'checkpoint_tread_history',
    'wheel_restore_core', 'checkpoint_wheel_phase', 'restore_thermal_plan',
    'checkpoint_brake_state', 'checkpoint_driveline_cache', 'checkpoint_driveline_outputs',
    'checkpoint_powertrain_control', 'checkpoint_shift_transient', 'physical_scoring_transaction',
    'restore_flow', 'lap_scoring_plan', 'native_memory_backend',
    'active_reset_control', 'active_reset_native'
) | ForEach-Object { Get-SectorNativeSource "${_}.c" }
$native = @(
    'vehicle_checkpoint', 'vehicle_state_reader',
    'vehicle_restore_adapter', 'contact_rebuild',
    'bodywork_checkpoint', 'graphics_lifetime',
    'native_lock_witness', 'native_lifetime_pin', 'native_lifetime_handoff',
    'fenced_restore', 'simulation_fence', 'active_reset_boundary'
) | ForEach-Object { Get-SectorNativeSource "${_}.cpp" }
$options = @('-O2', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated', '-target', 'x86_64-windows-gnu', "-DLMU_RESET_HOST_REVISION=$Revision")

$hostSources = @(
    'native_lifecycle_epoch.cpp', 'vehicle_lifecycle_hooks.cpp',
    'game_compatibility.cpp', 'physical_reset_host.cpp',
    'graphics_lifetime_hooks.cpp'
) | ForEach-Object { Get-SectorNativeSource $_ }
$minhook = Join-Path $projectRoot 'third_party\minhook'
$hostSources += @('src\buffer.c', 'src\hook.c', 'src\trampoline.c', 'src\hde\hde64.c') |
    ForEach-Object { Join-Path $minhook $_ }
$dll = Join-Path $projectRoot "bin/lmu-physical-reset-v$Revision-25661166.dll"
$fixtureDll = Join-Path $buildRoot 'physical-reset-host-fixture.dll'
& $ZigPath c++ @options -shared -DLMU_LIFECYCLE_FIXTURE -DLMU_RESET_HOST_FIXTURE -DLMU_VEHICLE_ADAPTER_FIXTURE -DLMU_PHASE_FIXTURE -I (Join-Path $minhook 'include') @core @native @hostSources -ladvapi32 -o $fixtureDll
if ($LASTEXITCODE -ne 0) { throw 'Physical host fixture DLL build failed' }
$fixtureExe = Join-Path $buildRoot 'physical-reset-host-fixture.exe'
& $ZigPath c++ @options -DLMU_VEHICLE_ADAPTER_FIXTURE @core @native (Get-SectorNativeSource 'physical_reset_host_fixture.cpp') -o $fixtureExe
if ($LASTEXITCODE -ne 0) { throw 'Physical host fixture build failed' }
if ($FrameDirectory) {
    & $fixtureExe $fixtureDll (Join-Path $FrameDirectory 'saved.frame')
    if ($LASTEXITCODE -ne 0) { throw 'Physical host fixture failed' }
    & $fixtureExe $fixtureDll (Join-Path $FrameDirectory 'saved.frame') 'no-player-callback'
    if ($LASTEXITCODE -ne 0) { throw 'Physical host discovery refusal fixture failed' }
    if ($Revision -ge 9) {
        & $fixtureExe $fixtureDll (Join-Path $FrameDirectory 'saved.frame') 'restart'
        if ($LASTEXITCODE -ne 0) { throw 'Physical host restart fixture failed' }
    }
}
& $ZigPath c++ @options -shared -I (Join-Path $minhook 'include') @core @native @hostSources -ladvapi32 -o $dll
if ($LASTEXITCODE -ne 0) { throw 'Physical reset host build failed' }
$targets = Get-Content (Get-SectorNativeSource 'reset_hook_targets.json') -Raw | ConvertFrom-Json
$targets = @($targets | Where-Object { !$_.minimum_revision -or $_.minimum_revision -le $Revision })
$manifest = [ordered]@{
    build = 14200
    dll_sha256 = (Get-FileHash $dll -Algorithm SHA256).Hash.ToLowerInvariant()
    targets = $targets
    experimental = $true
    maximum_speed_ms = $null
    maximum_displacement_m = $null
    writes_require_command = $true
}
[IO.File]::WriteAllText((Join-Path $projectRoot "compatibility/steam-25661166-physical-reset-v$Revision.json"),
    ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))

