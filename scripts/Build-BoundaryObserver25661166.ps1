param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$scoreWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $scoreWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $scoreWork 'zig-local-cache'
$scoreMh=Join-Path $projectRoot 'third_party\minhook'
$scoreSrc=Join-Path $projectRoot 'src'
$scoreBuild=Join-Path $scoreWork 'boundary-bin-25661166'
New-Item -ItemType Directory -Force -Path $scoreBuild | Out-Null
$scoreCommon=@('c++','-O2','-target','x86_64-windows-gnu','-I',(Join-Path $scoreMh 'include'),'-I',$scoreSrc)
$scoreSources=@((Join-Path $scoreSrc 'boundary_native_25661166.cpp'),(Join-Path $scoreMh 'src\buffer.c'),(Join-Path $scoreMh 'src\hook.c'),(Join-Path $scoreMh 'src\trampoline.c'),(Join-Path $scoreMh 'src\hde\hde64.c'))
& $ZigPath @scoreCommon '-shared' '-DLMU_PHASE_FIXTURE' @scoreSources '-ladvapi32' '-o' (Join-Path $scoreBuild 'boundary-fixture.dll')
if($LASTEXITCODE -ne 0){throw 'Phase fixture DLL build failed'}
& $ZigPath @scoreCommon (Join-Path $scoreSrc 'boundary_fixture_25661166.cpp') '-o' (Join-Path $scoreBuild 'boundary-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Phase fixture build failed'}
& (Join-Path $scoreBuild 'boundary-fixture.exe') (Join-Path $scoreBuild 'boundary-fixture.dll')
if($LASTEXITCODE -ne 0){throw 'Phase stop fixture failed'}
& (Join-Path $scoreBuild 'boundary-fixture.exe') (Join-Path $scoreBuild 'boundary-fixture.dll') 'watchdog'
if($LASTEXITCODE -ne 0){throw 'Phase watchdog fixture failed'}
& $ZigPath @scoreCommon '-shared' @scoreSources '-ladvapi32' '-o' (Join-Path $scoreBuild 'lmu-boundary-observer-25661166.dll')
if($LASTEXITCODE -ne 0){throw 'Phase DLL build failed'}
Copy-Item -LiteralPath (Join-Path $scoreBuild 'lmu-boundary-observer-25661166.dll') -Destination (Join-Path $projectRoot 'bin\lmu-boundary-observer-25661166.dll')
$phaseManifest=[ordered]@{
    utc=[DateTime]::UtcNow.ToString('o');build=14200;observation_only=$true;physics_writes=0
    dll_sha256=(Get-FileHash (Join-Path $projectRoot 'bin\lmu-boundary-observer-25661166.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
    targets=@(@{rva=0xecb4e0;prolog='488bc4488958104889701848897820';name='Dynamics'},@{rva=0xcf19f0;prolog='488bc448895820f30f114810555657';name='ScoreUpdate'},@{rva=0xcd1f40;prolog='488bc4488958104889701848897820';name='OuterUpdate'})
    fixture_stop_passed=$true;fixture_watchdog_passed=$true
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'compatibility\steam-25661166-boundary-observer.json'),($phaseManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
