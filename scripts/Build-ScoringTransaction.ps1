param([string]$ZigPath='')
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$scoringWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
if (-not $ZigPath) { $ZigPath=Join-Path $scoringWork 'tools\zig-x86_64-windows-0.16.0\zig.exe' }
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $scoringWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $scoringWork 'zig-local-cache'
$scoringFixture=Join-Path $scoringWork 'scoring-transaction-fixture.exe'
$scoringSources=@('scoring_transaction_fixture.c','scoring_plan_25661166.c','restore_transaction.c') | ForEach-Object {Join-Path $projectRoot "src\$_"}
& $ZigPath cc -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu @scoringSources -o $scoringFixture
if ($LASTEXITCODE -ne 0) { throw 'Scoring transaction fixture build failed' }
& $scoringFixture
if ($LASTEXITCODE -ne 0) { throw 'Scoring transaction fixture failed' }
