param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$poseWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$poseZig=$ZigPath
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $poseWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $poseWork 'zig-local-cache'
$poseMh=Join-Path $projectRoot 'third_party\minhook'
$poseSrc=Join-Path $projectRoot 'src'
$poseBuild=Join-Path $poseWork 'hinge-observer-bin'
$poseProfile=Get-Content (Join-Path $projectRoot 'compatibility\14200-telemetry.json') -Raw | ConvertFrom-Json
foreach($entry in $poseProfile.files) {
    if((Get-FileHash -LiteralPath (Join-Path $poseProfile.install_path $entry.relative_path) -Algorithm SHA256).Hash -ne $entry.sha256){throw '14200 build/header changed'}
}
New-Item -ItemType Directory -Force -Path $poseBuild | Out-Null
$poseCommon=@('c++','-O2','-target','x86_64-windows-gnu','-I',(Join-Path $poseMh 'include'),'-I',$poseSrc)
$poseSources=@((Join-Path $poseSrc 'hinge_observer_14200.cpp'),(Join-Path $poseSrc 'rigid_restore_core.c'),(Join-Path $poseSrc 'fuel_restore_core.c'),(Join-Path $poseSrc 'tire_inertia_core.c'),(Join-Path $poseSrc 'wheel_inertia_core.c'),(Join-Path $poseSrc 'mechanical_prepare_core.c'),(Join-Path $poseSrc 'bar_constraint_core.c'),(Join-Path $poseSrc 'angular_constraint_core.c'),(Join-Path $poseMh 'src\buffer.c'),(Join-Path $poseMh 'src\hook.c'),(Join-Path $poseMh 'src\trampoline.c'),(Join-Path $poseMh 'src\hde\hde64.c'))
& $poseZig @poseCommon '-shared' '-DLMU_POSE_FIXTURE' @poseSources '-ladvapi32' '-luser32' '-o' (Join-Path $poseBuild 'vehicle-fixture.dll')
if($LASTEXITCODE -ne 0){throw 'Pose fixture DLL build failed'}
& $poseZig @poseCommon (Join-Path $poseSrc 'hinge_observer_fixture_14200.cpp') (Join-Path $poseSrc 'rigid_restore_core.c') (Join-Path $poseSrc 'bar_constraint_core.c') (Join-Path $poseSrc 'angular_constraint_core.c') '-luser32' '-o' (Join-Path $poseBuild 'vehicle-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Pose fixture build failed'}
& $poseZig @poseCommon '-shared' @poseSources '-ladvapi32' '-luser32' '-o' (Join-Path $poseBuild 'lmu-hinge-observer-14200.dll')
if($LASTEXITCODE -ne 0){throw 'Pose observer build failed'}
& (Join-Path $poseBuild 'vehicle-fixture.exe') (Join-Path $poseBuild 'vehicle-fixture.dll')
if($LASTEXITCODE -ne 0){throw "Pose fixture failed: $LASTEXITCODE"}
& (Join-Path $poseBuild 'vehicle-fixture.exe') (Join-Path $poseBuild 'vehicle-fixture.dll') 'watchdog'
if($LASTEXITCODE -ne 0){throw "Pose watchdog fixture failed: $LASTEXITCODE"}
Copy-Item -LiteralPath (Join-Path $poseBuild 'lmu-hinge-observer-14200.dll') -Destination (Join-Path $projectRoot 'bin\lmu-hinge-observer-14200.dll')
$poseManifest=[ordered]@{
    utc=[DateTime]::UtcNow.ToString('o'); build=14200; observation_only=$true; physics_writes=0
    dll_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'bin\lmu-hinge-observer-14200.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
    targets=@(@{rva=0x3f5160; prolog='488bc448895810'; name='HingeJoint'})
    fixture_stop_passed=$true; fixture_watchdog_passed=$true
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'compatibility\14200-hinge-observer.json'),($poseManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
