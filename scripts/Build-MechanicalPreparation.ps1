param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$mechanicalWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$mechanicalSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $mechanicalWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $mechanicalWork 'zig-local-cache'
$mechanicalCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu',"$mechanicalSrc\rigid_restore_core.c","$mechanicalSrc\fuel_restore_core.c","$mechanicalSrc\tire_inertia_core.c","$mechanicalSrc\wheel_inertia_core.c","$mechanicalSrc\mechanical_prepare_core.c")
& $ZigPath @mechanicalCommon "$mechanicalSrc\mechanical_prepare_fixture.c" -o "$mechanicalWork\mechanical-prepare-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Mechanical fixture build failed'}
& "$mechanicalWork\mechanical-prepare-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Mechanical fixture failed'}
& $ZigPath @mechanicalCommon -shared "$mechanicalSrc\mechanical_prepare_bridge.c" -o "$mechanicalWork\mechanical-prepare-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Mechanical replay bridge failed'}
