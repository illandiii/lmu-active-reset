param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$barWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$barSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $barWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $barWork 'zig-local-cache'
$barCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu',"$barSrc\rigid_restore_core.c","$barSrc\bar_constraint_core.c")
& $ZigPath @barCommon "$barSrc\bar_constraint_fixture.c" -o "$barWork\bar-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Bar fixture build failed'}
& "$barWork\bar-constraint-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Bar fixture failed'}
& $ZigPath @barCommon -shared "$barSrc\bar_constraint_bridge.c" -o "$barWork\bar-constraint-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Bar replay bridge failed'}
