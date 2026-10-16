param([string]$ZigPath='')
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$lapWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
if (-not $ZigPath) { $ZigPath=Join-Path $lapWork 'tools\zig-x86_64-windows-0.16.0\zig.exe' }
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $lapWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $lapWork 'zig-local-cache'
$lapFixture=Join-Path $projectRoot 'bin\lap-guard-fixture.exe'
& $ZigPath cc -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu (Join-Path $projectRoot 'tests/native/lap_guard_fixture.c') -o $lapFixture
if ($LASTEXITCODE -ne 0) { throw 'Lap guard fixture build failed' }
& $lapFixture
if ($LASTEXITCODE -ne 0) { throw 'Lap guard fixture failed' }
