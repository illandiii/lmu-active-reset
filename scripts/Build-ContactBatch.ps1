param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$batchWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$batchSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $batchWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $batchWork 'zig-local-cache'
$batchSources=@('contact_batch_25661166.cpp','contact_batch_fixture.cpp','restore_flow.c','restore_transaction.c','scoring_plan_25661166.c') | ForEach-Object {Join-Path $batchSrc $_}
& $ZigPath c++ -O2 -Wall -Wextra -Werror -Wno-deprecated -target x86_64-windows-gnu @batchSources -o (Join-Path $batchWork 'contact-batch-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Contact batch build failed'}
& (Join-Path $batchWork 'contact-batch-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Contact batch fixture failed'}
