
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourcePaths.ps1')
$ErrorActionPreference='Stop'
$poseWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$poseZig=Join-Path $poseWork 'tools\zig-x86_64-windows-0.16.0\zig.exe'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $poseWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $poseWork 'zig-local-cache'
$poseMh=Join-Path $projectRoot 'third_party\minhook'
$poseSrc=Join-Path $projectRoot 'src'
$poseBuild=Join-Path $poseWork 'pose-bin'
$poseProfile=Get-Content (Join-Path $projectRoot 'compatibility\14150-telemetry.json') -Raw | ConvertFrom-Json
foreach($entry in $poseProfile.files) {
    if((Get-FileHash -LiteralPath (Join-Path $poseProfile.install_path $entry.relative_path) -Algorithm SHA256).Hash -ne $entry.sha256){throw '14150 build/header changed'}
}
New-Item -ItemType Directory -Force -Path $poseBuild | Out-Null
$poseCommon=@('c++','-O2','-target','x86_64-windows-gnu','-I',(Join-Path $poseMh 'include'),'-I',$poseSrc)
$poseSources=@((Get-SectorNativeSource 'pose_observer.cpp'),(Get-SectorNativeSource 'rigid_restore_core.c'),(Join-Path $poseMh 'src\buffer.c'),(Join-Path $poseMh 'src\hook.c'),(Join-Path $poseMh 'src\trampoline.c'),(Join-Path $poseMh 'src\hde\hde64.c'))
& $poseZig @poseCommon '-shared' '-DLMU_POSE_FIXTURE' @poseSources '-ladvapi32' '-luser32' '-o' (Join-Path $poseBuild 'pose-fixture.dll')
if($LASTEXITCODE -ne 0){throw 'Pose fixture DLL build failed'}
& $poseZig @poseCommon (Get-SectorNativeSource 'pose_observer_fixture.cpp') '-luser32' '-o' (Join-Path $poseBuild 'pose-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Pose fixture build failed'}
& $poseZig @poseCommon '-shared' @poseSources '-ladvapi32' '-luser32' '-o' (Join-Path $poseBuild 'lmu-pose-observer-14150.dll')
if($LASTEXITCODE -ne 0){throw 'Pose observer build failed'}
& (Join-Path $poseBuild 'pose-fixture.exe') (Join-Path $poseBuild 'pose-fixture.dll')
if($LASTEXITCODE -ne 0){throw "Pose fixture failed: $LASTEXITCODE"}
& (Join-Path $poseBuild 'pose-fixture.exe') (Join-Path $poseBuild 'pose-fixture.dll') 'watchdog'
if($LASTEXITCODE -ne 0){throw "Pose watchdog fixture failed: $LASTEXITCODE"}
Copy-Item -LiteralPath (Join-Path $poseBuild 'lmu-pose-observer-14150.dll') -Destination (Join-Path $projectRoot 'bin\lmu-pose-observer-14150.dll')
$poseManifest=[ordered]@{
    utc=[DateTime]::UtcNow.ToString('o'); build=14150; observation_only=$true; physics_writes=0
    dll_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'bin\lmu-pose-observer-14150.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
    targets=@(
        @{rva=0xeb79e0; prolog='488bc4488958104889701848897820'; name='Dynamics'}
        @{rva=0xee5030; prolog='4c894c242048894c2408555356574154415541564157'; name='Contacts'}
        @{rva=0xeb7030; prolog='488bc44889580848897010488978184c897020'; name='Forces'}
    )
    fixture_stop_passed=$true; fixture_watchdog_passed=$true
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'compatibility\14150-pose-observer.json'),($poseManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
