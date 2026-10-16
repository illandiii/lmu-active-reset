param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourcePaths.ps1')

$ErrorActionPreference='Stop'
$fixtureWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $fixtureWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $fixtureWork 'zig-local-cache'
& $ZigPath cc -O2 -Wall -Wextra -Werror (Get-SectorNativeSource 'checkpoint_driveline_outputs.c') (Get-SectorNativeSource 'checkpoint_driveline_outputs_fixture.c') (Get-SectorNativeSource 'restore_transaction.c') -o (Join-Path $fixtureWork 'driveline-outputs-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Driveline outputs fixture build failed'}
& (Join-Path $fixtureWork 'driveline-outputs-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Driveline outputs fixture failed'}
