param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')

$ErrorActionPreference='Stop'
$ballWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $ballWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $ballWork 'zig-local-cache'
$ballCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu',(Get-SectorNativeSource 'rigid_restore_core.c'),(Get-SectorNativeSource 'bar_constraint_core.c'),(Get-SectorNativeSource 'ball_constraint_core.c'))
& $ZigPath @ballCommon (Get-SectorNativeSource 'ball_constraint_fixture.c') -o "$ballWork\ball-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Ball fixture build failed'}
& "$ballWork\ball-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Ball fixture failed'}
& $ZigPath @ballCommon -shared (Get-SectorNativeSource 'ball_constraint_bridge.c') -o "$ballWork\ball-constraint-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Ball replay bridge failed'}
