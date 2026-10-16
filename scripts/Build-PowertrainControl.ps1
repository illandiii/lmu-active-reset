param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourcePaths.ps1')

$ErrorActionPreference='Stop'
$jointWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $jointWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $jointWork 'zig-local-cache'
$jointSources=@('checkpoint_powertrain_control.c','checkpoint_powertrain_control_fixture.c','restore_transaction.c') | ForEach-Object {Get-SectorNativeSource $_}
& $ZigPath cc -O2 -Wall -Wextra -Werror @jointSources -o (Join-Path $jointWork 'powertrain-control-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Powertrain control build failed'}
& (Join-Path $jointWork 'powertrain-control-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Powertrain control fixture failed'}
