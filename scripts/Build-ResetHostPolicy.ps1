param([Parameter(Mandatory = $true)][string]$ZigPath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$workRoot = Join-Path $projectRoot 'work'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $workRoot 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $workRoot 'zig-local-cache'
$fixture = Join-Path $workRoot 'reset-host-policy-fixture.exe'
& $ZigPath c++ -std=c++17 -O2 -Wall -Wextra -Werror (Join-Path $projectRoot 'tests/native/reset_host_policy_fixture.cpp') -o $fixture
if ($LASTEXITCODE -ne 0) { throw 'Host duration policy compilation failed' }
& $fixture
if ($LASTEXITCODE -ne 0) { throw 'Host duration policy fixture failed' }
