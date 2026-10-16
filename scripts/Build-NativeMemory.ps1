param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourcePaths.ps1')

$ErrorActionPreference='Stop'
$nativeWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $nativeWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $nativeWork 'zig-local-cache'
$nativeSources=@('restore_native_memory_fixture.c','restore_native_memory_25661166.c','restore_bindings_25661166.c','restore_transaction.c','restore_flow.c','scoring_plan_25661166.c') | ForEach-Object {Get-SectorNativeSource $_}
& $ZigPath cc -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu @nativeSources -o (Join-Path $nativeWork 'restore-native-memory-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Native memory fixture build failed'}
& (Join-Path $nativeWork 'restore-native-memory-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Native memory fixture failed'}
