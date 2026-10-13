param([Parameter(Mandatory = $true)][string]$ZigPath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$workRoot = Join-Path $projectRoot 'work'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $workRoot 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $workRoot 'zig-local-cache'
$sources = @('native_lock_witness.cpp', 'native_lifetime_pin.cpp', 'native_lifetime_pin_fixture.cpp') |
    ForEach-Object { Join-Path $projectRoot "src\$_" }
$fixture = Join-Path $workRoot 'native-lifetime-pin-fixture.exe'
& $ZigPath c++ -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu @sources -o $fixture
if ($LASTEXITCODE -ne 0) { throw 'Native lifetime pin build failed' }
& $fixture
if ($LASTEXITCODE -ne 0) { throw 'Native lifetime pin fixture failed' }
