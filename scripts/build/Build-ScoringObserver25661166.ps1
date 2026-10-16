param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')

$ErrorActionPreference='Stop'
$scoreWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $scoreWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $scoreWork 'zig-local-cache'
$scoreMh=Join-Path $projectRoot 'third_party\minhook'
$scoreSrc=Join-Path $projectRoot 'src'
$scoreBuild=Join-Path $scoreWork 'scoring-bin-25661166'
New-Item -ItemType Directory -Force -Path $scoreBuild | Out-Null
$scoreCommon=@('c++','-O2','-target','x86_64-windows-gnu','-I',(Join-Path $scoreMh 'include'),'-I',$scoreSrc)
$scoreSources=@((Get-SectorNativeSource 'scoring_native_25661166.cpp'),(Join-Path $scoreMh 'src\buffer.c'),(Join-Path $scoreMh 'src\hook.c'),(Join-Path $scoreMh 'src\trampoline.c'),(Join-Path $scoreMh 'src\hde\hde64.c'))
& $ZigPath @scoreCommon '-shared' '-DLMU_SCORING_FIXTURE' @scoreSources '-ladvapi32' '-o' (Join-Path $scoreBuild 'scoring-fixture.dll')
if($LASTEXITCODE -ne 0){throw 'Scoring fixture DLL build failed'}
& $ZigPath @scoreCommon (Get-SectorNativeSource 'scoring_fixture_25661166.cpp') '-o' (Join-Path $scoreBuild 'scoring-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Scoring fixture build failed'}
& (Join-Path $scoreBuild 'scoring-fixture.exe') (Join-Path $scoreBuild 'scoring-fixture.dll')
if($LASTEXITCODE -ne 0){throw 'Scoring stop fixture failed'}
& (Join-Path $scoreBuild 'scoring-fixture.exe') (Join-Path $scoreBuild 'scoring-fixture.dll') 'watchdog'
if($LASTEXITCODE -ne 0){throw 'Scoring watchdog fixture failed'}
& $ZigPath @scoreCommon '-shared' @scoreSources '-ladvapi32' '-o' (Join-Path $scoreBuild 'lmu-scoring-observer-25661166.dll')
if($LASTEXITCODE -ne 0){throw 'Scoring DLL build failed'}
Copy-Item -LiteralPath (Join-Path $scoreBuild 'lmu-scoring-observer-25661166.dll') -Destination (Join-Path $projectRoot 'bin\lmu-scoring-observer-25661166.dll')
$scoreHashes=[ordered]@{}
foreach($scoreName in @('scoring_native_25661166.cpp','scoring_ipc_25661166.h','scoring_fixture_25661166.cpp','lap_guard_core.h')){$scoreHashes[$scoreName]=(Get-FileHash -LiteralPath (Get-SectorNativeSource $scoreName) -Algorithm SHA256).Hash.ToLowerInvariant()}
$scoreManifest=[ordered]@{
 utc=[DateTime]::UtcNow.ToString('o');steam_build=25661166;app_build=14200
 exe_sha256='66942337813b301b87d12edfa330f33354a2a4b8cd9b048885cbf77eb6f940cf'
 dll_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'bin\lmu-scoring-observer-25661166.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
 targets=@(@{name='ScoreUpdate';rva=0xcf19f0;prolog='488bc448895820f30f114810555657'})
 abi='void(score* RCX,float dt XMM1); caller CC86BF ignores return; observed input use in CF19F0'
 observation_only=$true;full_restore_supported=$false;source_sha256=$scoreHashes;fixture_stop_passed=$true;fixture_watchdog_passed=$true
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'compatibility\steam-25661166-scoring-observer.json'),($scoreManifest|ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
