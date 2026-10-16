param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')

$ErrorActionPreference='Stop'
$groupWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $groupWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $groupWork 'zig-local-cache'
$groupCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu')
$groupGeometry=@((Get-SectorNativeSource 'rigid_restore_core.c'),(Get-SectorNativeSource 'bar_constraint_core.c'),(Get-SectorNativeSource 'ball_constraint_core.c'),(Get-SectorNativeSource 'angular_constraint_core.c'),(Get-SectorNativeSource 'constraint_group_core.c'))
$groupMechanical=@((Get-SectorNativeSource 'fuel_restore_core.c'),(Get-SectorNativeSource 'tire_inertia_core.c'),(Get-SectorNativeSource 'wheel_inertia_core.c'),(Get-SectorNativeSource 'mechanical_prepare_core.c'),(Get-SectorNativeSource 'mechanical_constraint_core.c'))
& $ZigPath @groupCommon @groupGeometry @groupMechanical (Get-SectorNativeSource 'mechanical_constraint_fixture.c') -o "$groupWork\mechanical-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Integrated fixture build failed'}
& "$groupWork\mechanical-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Integrated fixture failed'}
& $ZigPath @groupCommon @groupGeometry -shared (Get-SectorNativeSource 'constraint_group_bridge.c') -o "$groupWork\constraint-group-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Group replay bridge failed'}
& $ZigPath @groupCommon @groupGeometry @groupMechanical -shared (Get-SectorNativeSource 'mechanical_constraint_bridge.c') -o "$groupWork\mechanical-constraint-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Integrated replay bridge failed'}
