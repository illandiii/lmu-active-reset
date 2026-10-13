param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$jointWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$jointSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $jointWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $jointWork 'zig-local-cache'
$jointSources=@('restore_joint_25661166.c','restore_joint_fixture.c','scoring_plan_25661166.c','restore_transaction.c') | ForEach-Object {Join-Path $jointSrc $_}
& $ZigPath cc -O2 -Wall -Wextra -Werror @jointSources -o (Join-Path $jointWork 'joint-restore-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Joint restore build failed'}
& (Join-Path $jointWork 'joint-restore-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Joint restore fixture failed'}
