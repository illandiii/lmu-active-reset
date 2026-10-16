param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')

$ErrorActionPreference='Stop'
$batchWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $batchWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $batchWork 'zig-local-cache'
$batchSources=@('contact_batch_25661166.cpp','contact_batch_fixture.cpp','restore_flow.c','restore_transaction.c','scoring_plan_25661166.c') | ForEach-Object {Get-SectorNativeSource $_}
& $ZigPath c++ -O2 -Wall -Wextra -Werror -Wno-deprecated -target x86_64-windows-gnu @batchSources -o (Join-Path $batchWork 'contact-batch-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Contact batch build failed'}
& (Join-Path $batchWork 'contact-batch-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Contact batch fixture failed'}
