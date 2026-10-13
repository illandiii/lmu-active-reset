param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$poseWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$poseZig=$ZigPath
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $poseWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $poseWork 'zig-local-cache'
$poseMh=Join-Path $projectRoot 'third_party\minhook'
$poseSrc=Join-Path $projectRoot 'src'
$poseBuild=Join-Path $poseWork 'binding-bin'
$poseProfile=Get-Content (Join-Path $projectRoot 'compatibility\14200-telemetry.json') -Raw | ConvertFrom-Json
foreach($entry in $poseProfile.files) {
    if((Get-FileHash -LiteralPath (Join-Path $poseProfile.install_path $entry.relative_path) -Algorithm SHA256).Hash -ne $entry.sha256){throw '14200 build/header changed'}
}
New-Item -ItemType Directory -Force -Path $poseBuild | Out-Null
$poseCommon=@('c++','-O2','-target','x86_64-windows-gnu','-I',(Join-Path $poseMh 'include'),'-I',$poseSrc)
$poseSources=@((Join-Path $poseSrc 'binding_observer.cpp'),(Join-Path $poseSrc 'rigid_restore_core.c'),(Join-Path $poseSrc 'wheel_restore_core.c'),(Join-Path $poseMh 'src\buffer.c'),(Join-Path $poseMh 'src\hook.c'),(Join-Path $poseMh 'src\trampoline.c'),(Join-Path $poseMh 'src\hde\hde64.c'))
& $poseZig @poseCommon '-shared' '-DLMU_BINDING_FIXTURE' @poseSources '-ladvapi32' '-luser32' '-o' (Join-Path $poseBuild 'binding-fixture.dll') 2> (Join-Path $poseWork 'binding-build.log')
if($LASTEXITCODE -ne 0){throw 'Pose fixture DLL build failed'}
& $poseZig @poseCommon (Join-Path $poseSrc 'binding_observer_fixture.cpp') '-luser32' '-o' (Join-Path $poseBuild 'binding-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Pose fixture build failed'}
& $poseZig @poseCommon '-shared' @poseSources '-ladvapi32' '-luser32' '-o' (Join-Path $poseBuild 'lmu-binding-observer-14200.dll')
if($LASTEXITCODE -ne 0){throw 'Pose observer build failed'}
& (Join-Path $poseBuild 'binding-fixture.exe') (Join-Path $poseBuild 'binding-fixture.dll')
if($LASTEXITCODE -ne 0){throw "Pose fixture failed: $LASTEXITCODE"}
& (Join-Path $poseBuild 'binding-fixture.exe') (Join-Path $poseBuild 'binding-fixture.dll') 'watchdog'
if($LASTEXITCODE -ne 0){throw "Pose watchdog fixture failed: $LASTEXITCODE"}
Copy-Item -LiteralPath (Join-Path $poseBuild 'lmu-binding-observer-14200.dll') -Destination (Join-Path $projectRoot 'bin\lmu-binding-observer-14200.dll')
$poseManifest=[ordered]@{
    utc=[DateTime]::UtcNow.ToString('o'); build=14200; observation_only=$true; physics_writes=0
    dll_sha256=(Get-FileHash -LiteralPath (Join-Path $projectRoot 'bin\lmu-binding-observer-14200.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
    targets=@(
        @{rva=0xec87e0; prolog='488bc4488958104889701848897820'; name='Dynamics'}
        @{rva=0xee1310; prolog='488bc44889581048897818'; name='WheelTransform'}
    )
    fixture_stop_passed=$true; fixture_watchdog_passed=$true
}
[IO.File]::WriteAllText((Join-Path $projectRoot 'compatibility\14200-binding-observer.json'),($poseManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
