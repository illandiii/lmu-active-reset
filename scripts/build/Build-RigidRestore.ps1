param([string]$ZigPath='')
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$ErrorActionPreference='Stop'
$rigidWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
if (-not $ZigPath) { $ZigPath=Join-Path $rigidWork 'tools\zig-x86_64-windows-0.16.0\zig.exe' }
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $rigidWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $rigidWork 'zig-local-cache'
$rigidFixture=Join-Path $projectRoot 'bin\rigid-restore-fixture.exe'
& $ZigPath cc -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu (Join-Path $projectRoot 'src/native/physics/rigid_restore_core.c') (Join-Path $projectRoot 'tests/native/rigid_restore_fixture.c') -o $rigidFixture
if ($LASTEXITCODE -ne 0) { throw 'Rigid restore fixture build failed' }
& $rigidFixture
if ($LASTEXITCODE -ne 0) { throw 'Rigid restore fixture failed' }
