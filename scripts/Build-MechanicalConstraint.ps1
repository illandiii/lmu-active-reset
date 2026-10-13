param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$groupWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$groupSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $groupWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $groupWork 'zig-local-cache'
$groupCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu')
$groupGeometry=@("$groupSrc\rigid_restore_core.c","$groupSrc\bar_constraint_core.c","$groupSrc\ball_constraint_core.c","$groupSrc\angular_constraint_core.c","$groupSrc\constraint_group_core.c")
$groupMechanical=@("$groupSrc\fuel_restore_core.c","$groupSrc\tire_inertia_core.c","$groupSrc\wheel_inertia_core.c","$groupSrc\mechanical_prepare_core.c","$groupSrc\mechanical_constraint_core.c")
& $ZigPath @groupCommon @groupGeometry @groupMechanical "$groupSrc\mechanical_constraint_fixture.c" -o "$groupWork\mechanical-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Integrated fixture build failed'}
& "$groupWork\mechanical-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Integrated fixture failed'}
& $ZigPath @groupCommon @groupGeometry -shared "$groupSrc\constraint_group_bridge.c" -o "$groupWork\constraint-group-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Group replay bridge failed'}
& $ZigPath @groupCommon @groupGeometry @groupMechanical -shared "$groupSrc\mechanical_constraint_bridge.c" -o "$groupWork\mechanical-constraint-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Integrated replay bridge failed'}
