param(
    [Parameter(Mandatory = $true)][string]$ZigPath,
    [string]$CaptureDirectory
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$buildRoot = Join-Path $projectRoot 'work'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $buildRoot 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $buildRoot 'zig-local-cache'
$fixture = Join-Path $buildRoot 'bodywork-restore-fixture.exe'

# This fixture owns every buffer and lock. It never opens or modifies LMU.
& $ZigPath c++ -std=c++17 -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu `
    (Join-Path $projectRoot 'src/native/restore/bodywork_checkpoint.cpp') `
    (Join-Path $projectRoot 'tests/native/bodywork_restore_fixture.cpp') -o $fixture
if ($LASTEXITCODE -ne 0) { throw 'Bodywork fixture build failed' }
if ($CaptureDirectory) {
    & $fixture $CaptureDirectory
} else {
    & $fixture
}
if ($LASTEXITCODE -ne 0) { throw 'Bodywork fixture failed' }

$lifetimeFixture = Join-Path $buildRoot 'graphics-lifetime-fixture.exe'
& $ZigPath c++ -std=c++17 -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu `
    (Join-Path $projectRoot 'src/native/runtime/graphics_lifetime.cpp') `
    (Join-Path $projectRoot 'tests/native/graphics_lifetime_fixture.cpp') -o $lifetimeFixture
if ($LASTEXITCODE -ne 0) { throw 'Graphics lifetime fixture build failed' }
& $lifetimeFixture
if ($LASTEXITCODE -ne 0) { throw 'Graphics lifetime fixture failed' }
