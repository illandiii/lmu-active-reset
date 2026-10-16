param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')

$ErrorActionPreference='Stop'
$angularWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $angularWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $angularWork 'zig-local-cache'
$angularCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu',(Get-SectorNativeSource 'rigid_restore_core.c'),(Get-SectorNativeSource 'bar_constraint_core.c'),(Get-SectorNativeSource 'angular_constraint_core.c'))
& $ZigPath @angularCommon (Get-SectorNativeSource 'angular_constraint_fixture.c') -o "$angularWork\angular-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Angular fixture build failed'}
& "$angularWork\angular-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Angular fixture failed'}
& $ZigPath @angularCommon -shared (Get-SectorNativeSource 'angular_constraint_bridge.c') -o "$angularWork\angular-constraint-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Angular replay bridge failed'}

