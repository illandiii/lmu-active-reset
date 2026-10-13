param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$fixtureWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$fixtureSource=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $fixtureWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $fixtureWork 'zig-local-cache'
& $ZigPath cc -O2 -Wall -Wextra -Werror (Join-Path $fixtureSource 'checkpoint_driveline_outputs.c') (Join-Path $fixtureSource 'checkpoint_driveline_outputs_fixture.c') (Join-Path $fixtureSource 'restore_transaction.c') -o (Join-Path $fixtureWork 'driveline-outputs-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Driveline outputs fixture build failed'}
& (Join-Path $fixtureWork 'driveline-outputs-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Driveline outputs fixture failed'}
