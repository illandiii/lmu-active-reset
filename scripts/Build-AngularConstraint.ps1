param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$angularWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$angularSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $angularWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $angularWork 'zig-local-cache'
$angularCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu',"$angularSrc\rigid_restore_core.c","$angularSrc\bar_constraint_core.c","$angularSrc\angular_constraint_core.c")
& $ZigPath @angularCommon "$angularSrc\angular_constraint_fixture.c" -o "$angularWork\angular-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Angular fixture build failed'}
& "$angularWork\angular-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Angular fixture failed'}
& $ZigPath @angularCommon -shared "$angularSrc\angular_constraint_bridge.c" -o "$angularWork\angular-constraint-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Angular replay bridge failed'}

