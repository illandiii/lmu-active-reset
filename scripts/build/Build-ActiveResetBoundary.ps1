param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'dev/SourcePaths.ps1')

$ErrorActionPreference='Stop'
$boundaryWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $boundaryWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $boundaryWork 'zig-local-cache'
$boundarySources=@('active_reset_boundary.cpp','active_reset_boundary_fixture.cpp','active_reset_native.c','active_reset_control.c','simulation_fence.cpp','native_lock_witness.cpp','fenced_restore.cpp','restore_native_memory_25661166.c','restore_bindings_25661166.c','restore_flow.c','restore_transaction.c','scoring_plan_25661166.c') | ForEach-Object {Get-SectorNativeSource $_}
& $ZigPath c++ -O2 -Wall -Wextra -Werror -Wno-deprecated -target x86_64-windows-gnu @boundarySources -o (Join-Path $boundaryWork 'active-reset-boundary-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Boundary integration build failed'}
& (Join-Path $boundaryWork 'active-reset-boundary-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Boundary integration fixture failed'}
