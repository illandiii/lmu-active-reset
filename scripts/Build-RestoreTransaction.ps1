param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourcePaths.ps1')

$ErrorActionPreference='Stop'
$restoreWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $restoreWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $restoreWork 'zig-local-cache'
& $ZigPath cc -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu (Get-SectorNativeSource 'restore_transaction.c') (Get-SectorNativeSource 'restore_transaction_fixture.c') -o "$restoreWork\restore-transaction-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Transaction build failed'}
& "$restoreWork\restore-transaction-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Transaction fixture failed'}
