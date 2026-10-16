param([Parameter(Mandatory = $true)][string]$ZigPath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$workRoot = Join-Path $projectRoot 'work'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $workRoot 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $workRoot 'zig-local-cache'
$fixture = Join-Path $workRoot 'physical-reset-trace-fixture.exe'
& $ZigPath c++ -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu -DLMU_RESET_HOST_REVISION=14 (Join-Path $projectRoot 'tests/native/physical_reset_trace_fixture.cpp') -o $fixture
if ($LASTEXITCODE -ne 0) { throw 'Physical trace fixture build failed' }
& $fixture
if ($LASTEXITCODE -ne 0) { throw 'Physical trace fixture failed' }
