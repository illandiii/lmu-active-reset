param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$flowWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$flowSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $flowWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $flowWork 'zig-local-cache'
$flowSources=@('restore_flow.c','restore_flow_fixture.c','restore_transaction.c','scoring_plan_25661166.c') | ForEach-Object {Join-Path $flowSrc $_}
& $ZigPath cc -O2 -Wall -Wextra -Werror @flowSources -o (Join-Path $flowWork 'restore-flow-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Restore flow build failed'}
& (Join-Path $flowWork 'restore-flow-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Restore flow fixture failed'}
