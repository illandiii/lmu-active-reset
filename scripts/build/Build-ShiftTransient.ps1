param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')

$ErrorActionPreference='Stop'
$jointWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $jointWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $jointWork 'zig-local-cache'
$jointSources=@('checkpoint_shift_transient.c','checkpoint_shift_transient_fixture.c','restore_transaction.c') | ForEach-Object {Get-SectorNativeSource $_}
& $ZigPath cc -O2 -Wall -Wextra -Werror @jointSources -o (Join-Path $jointWork 'shift-transient-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Shift transient build failed'}
& (Join-Path $jointWork 'shift-transient-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Shift transient fixture failed'}

