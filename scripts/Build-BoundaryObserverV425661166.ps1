param(
    [Parameter(Mandatory = $true)][string]$ZigPath,
    [switch]$FixturesOnly
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$workRoot = Join-Path $projectRoot 'work'
$sourceRoot = Join-Path $projectRoot 'src'
$minhookRoot = Join-Path $projectRoot 'third_party\minhook'
$buildRoot = Join-Path $workRoot 'boundary-v4-bin-25661166'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $workRoot 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $workRoot 'zig-local-cache'
New-Item -ItemType Directory -Force -Path $buildRoot | Out-Null
$options = @(
    'c++', '-DLMU_BOUNDARY_HANDOFF', '-O2', '-target', 'x86_64-windows-gnu',
    '-I', (Join-Path $minhookRoot 'include'), '-I', $sourceRoot
)
$sources = @(
    'native_lifetime_handoff.cpp', 'native_lifetime_pin.cpp',
    'native_lock_witness.cpp', 'boundary_phase_observer_25661166.cpp'
) | ForEach-Object { Join-Path $sourceRoot $_ }
$sources += @('src\buffer.c', 'src\hook.c', 'src\trampoline.c', 'src\hde\hde64.c') |
    ForEach-Object { Join-Path $minhookRoot $_ }
$fixtureDll = Join-Path $buildRoot 'boundary-fixture.dll'
$fixtureExe = Join-Path $buildRoot 'boundary-fixture.exe'
& $ZigPath @options -shared -DLMU_PHASE_FIXTURE @sources -ladvapi32 -o $fixtureDll
if ($LASTEXITCODE -ne 0) { throw 'Boundary fixture DLL build failed' }
& $ZigPath @options (Join-Path $sourceRoot 'boundary_fixture_v3_25661166.cpp') -o $fixtureExe
if ($LASTEXITCODE -ne 0) { throw 'Boundary fixture build failed' }
& $fixtureExe $fixtureDll
if ($LASTEXITCODE -ne 0) { throw 'Boundary stop fixture failed' }
& $fixtureExe $fixtureDll watchdog
if ($LASTEXITCODE -ne 0) { throw 'Boundary watchdog fixture failed' }
if ($FixturesOnly) { return }
$dllName = 'lmu-boundary-observer-v4-25661166.dll'
$builtDll = Join-Path $buildRoot $dllName
$installedDll = Join-Path $projectRoot "bin\$dllName"
& $ZigPath @options -shared @sources -ladvapi32 -o $builtDll
if ($LASTEXITCODE -ne 0) { throw 'Boundary DLL build failed' }
Copy-Item -LiteralPath $builtDll -Destination $installedDll
$manifest = [ordered]@{
    utc = [DateTime]::UtcNow.ToString('o')
    build = 14200
    observation_only = $true
    physics_writes = 0
    asynchronous_admission = $true
    admission_budget_ms = 25
    admission_interval_ms = 500
    dll_sha256 = (Get-FileHash $installedDll -Algorithm SHA256).Hash.ToLowerInvariant()
    targets = @(
        @{ rva = 0xecb4e0; prolog = '488bc4488958104889701848897820'; name = 'Dynamics' },
        @{ rva = 0xcf19f0; prolog = '488bc448895820f30f114810555657'; name = 'ScoreUpdate' },
        @{ rva = 0xcd1f40; prolog = '488bc4488958104889701848897820'; name = 'OuterUpdate' }
    )
    fixture_stop_passed = $true
    fixture_watchdog_passed = $true
}
[IO.File]::WriteAllText(
    (Join-Path $projectRoot 'compatibility\steam-25661166-boundary-observer-v4.json'),
    ($manifest | ConvertTo-Json -Depth 5),
    [Text.UTF8Encoding]::new($false)
)
