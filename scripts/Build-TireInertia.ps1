param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourcePaths.ps1')

$ErrorActionPreference='Stop'
$tireWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $tireWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $tireWork 'zig-local-cache'
$tireCommon=@('cc','-O2','-Wall','-Wextra','-Werror','-target','x86_64-windows-gnu',(Get-SectorNativeSource 'tire_inertia_core.c'))
& $ZigPath @tireCommon (Get-SectorNativeSource 'tire_inertia_fixture.c') -o "$tireWork\tire-inertia-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Tire inertia build failed'}
& "$tireWork\tire-inertia-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Tire inertia fixture failed'}
& $ZigPath @tireCommon -shared (Get-SectorNativeSource 'tire_inertia_bridge.c') -o "$tireWork\tire-inertia-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Tire inertia replay bridge failed'}
