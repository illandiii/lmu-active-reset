
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')
$ErrorActionPreference='Stop'
$kinWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$kinZig=Join-Path $kinWork 'tools\zig-x86_64-windows-0.16.0\zig.exe'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $kinWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $kinWork 'zig-local-cache'
$kinSrc=Join-Path $projectRoot 'src'
$kinBuild=Join-Path $kinWork 'kinematic-bin'
New-Item -ItemType Directory -Force -Path $kinBuild | Out-Null
$kinCommon=@('cc','-O2','-target','x86_64-windows-gnu','-I',$kinSrc)
& $kinZig @kinCommon (Get-SectorNativeSource 'rigid_restore_core.c') (Get-SectorNativeSource 'kinematic_current_mass_fixture.c') '-o' (Join-Path $kinBuild 'kinematic-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Kinematic fixture build failed'}
& (Join-Path $kinBuild 'kinematic-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Kinematic analytical test failed'}
& $kinZig @kinCommon '-shared' (Get-SectorNativeSource 'rigid_restore_core.c') (Get-SectorNativeSource 'kinematic_fixture_bridge.c') '-o' (Join-Path $kinBuild 'kinematic-fixture.dll')
if($LASTEXITCODE -ne 0){throw 'Local kinematic bridge build failed'}
