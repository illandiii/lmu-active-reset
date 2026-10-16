
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')
$ErrorActionPreference='Stop'
$observerWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$zigObserver=Join-Path $observerWork 'tools\zig-x86_64-windows-0.16.0\zig.exe'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $observerWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $observerWork 'zig-local-cache'
$mhObserver=Join-Path $projectRoot 'third_party\minhook'
$observerBuild=Join-Path $observerWork 'observer-bin'
$observerSources=Join-Path $projectRoot 'src'
New-Item -ItemType Directory -Force -Path $observerBuild | Out-Null
$commonObserver=@('cc','-O2','-target','x86_64-windows-gnu','-I',(Join-Path $mhObserver 'include'),'-I',$observerSources)
$sourcesObserver=@((Get-SectorNativeSource 'physics_observer.c'),(Join-Path $mhObserver 'src\buffer.c'),(Join-Path $mhObserver 'src\hook.c'),(Join-Path $mhObserver 'src\trampoline.c'),(Join-Path $mhObserver 'src\hde\hde64.c'))
& $zigObserver @commonObserver '-shared' '-DLMU_OBSERVER_FIXTURE' @sourcesObserver '-ladvapi32' '-luser32' '-o' (Join-Path $observerBuild 'observer-fixture.dll')
if($LASTEXITCODE -ne 0){throw 'Fixture DLL build failed'}
& $zigObserver @commonObserver (Get-SectorNativeSource 'observer_fixture.c') '-luser32' '-o' (Join-Path $observerBuild 'observer-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Fixture executable build failed'}
& $zigObserver @commonObserver '-shared' @sourcesObserver '-ladvapi32' '-luser32' '-o' (Join-Path $observerBuild 'lmu-physics-observer.dll')
if($LASTEXITCODE -ne 0){throw 'Game observer DLL build failed'}
Copy-Item -LiteralPath (Join-Path $observerBuild 'lmu-physics-observer.dll') -Destination (Join-Path $projectRoot 'bin\lmu-physics-observer.dll')
Write-Output 'Observer and fixture builds completed.'
