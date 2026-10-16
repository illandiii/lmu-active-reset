param([Parameter(Mandatory = $true)][string]$ZigPath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')
$workRoot = Join-Path $projectRoot 'work'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $workRoot 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $workRoot 'zig-local-cache'
$sources = @('native_lock_witness.cpp', 'native_lifecycle_epoch.cpp', 'native_lifecycle_epoch_fixture.cpp') |
    ForEach-Object { Get-SectorNativeSource $_ }
$fixture = Join-Path $workRoot 'native-lifecycle-epoch-fixture.exe'
& $ZigPath c++ -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu @sources -o $fixture
if ($LASTEXITCODE -ne 0) { throw 'Lifecycle epoch build failed' }
& $fixture
if ($LASTEXITCODE -ne 0) { throw 'Lifecycle epoch fixture failed' }

$minhook = Join-Path $projectRoot 'third_party\minhook'
$hookSources = @('native_lock_witness.cpp', 'native_lifecycle_epoch.cpp', 'native_lifecycle_hooks_25661166.cpp', 'native_lifecycle_hooks_fixture.cpp') |
    ForEach-Object { Get-SectorNativeSource $_ }
$hookSources += @('src\buffer.c', 'src\hook.c', 'src\trampoline.c', 'src\hde\hde64.c') |
    ForEach-Object { Join-Path $minhook $_ }
$hookFixture = Join-Path $workRoot 'native-lifecycle-hooks-fixture.exe'
& $ZigPath c++ -O2 -target x86_64-windows-gnu -DLMU_LIFECYCLE_FIXTURE -I (Join-Path $minhook 'include') @hookSources -o $hookFixture
if ($LASTEXITCODE -ne 0) { throw 'Lifecycle hook fixture build failed' }
& $hookFixture
if ($LASTEXITCODE -ne 0) { throw 'Lifecycle hook fixture failed' }
