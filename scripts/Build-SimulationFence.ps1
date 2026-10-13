param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$fenceWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$fenceSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $fenceWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $fenceWork 'zig-local-cache'
& $ZigPath c++ -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu (Join-Path $fenceSrc 'simulation_fence.cpp') (Join-Path $fenceSrc 'simulation_fence_fixture.cpp') -o (Join-Path $fenceWork 'simulation-fence-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Fence build failed'}
& (Join-Path $fenceWork 'simulation-fence-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Fence fixture failed'}

$fencedSources=@('simulation_fence.cpp','native_lock_witness.cpp','fenced_restore.cpp','fenced_restore_fixture.cpp','restore_flow.c','restore_transaction.c','scoring_plan_25661166.c') | ForEach-Object {Join-Path $fenceSrc $_}
& $ZigPath c++ -O2 -Wall -Wextra -Werror -Wno-deprecated -target x86_64-windows-gnu @fencedSources -o (Join-Path $fenceWork 'fenced-restore-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Fenced restore build failed'}
& (Join-Path $fenceWork 'fenced-restore-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Fenced restore fixture failed'}

$fenceMh=Join-Path $projectRoot 'third_party\minhook'
$hookSources=@('simulation_fence.cpp','native_lock_witness.cpp','fenced_restore.cpp','fenced_hooks_fixture.cpp','restore_flow.c','restore_transaction.c','scoring_plan_25661166.c') | ForEach-Object {Join-Path $fenceSrc $_}
$hookSources+=@('src\buffer.c','src\hook.c','src\trampoline.c','src\hde\hde64.c') | ForEach-Object {Join-Path $fenceMh $_}
& $ZigPath c++ -O2 -Wall -Wextra -Wno-deprecated -target x86_64-windows-gnu -I (Join-Path $fenceMh 'include') @hookSources -o (Join-Path $fenceWork 'fenced-hooks-fixture.exe')
if($LASTEXITCODE -ne 0){throw 'Fenced hook build failed'}
foreach($faultCase in 0..6){
    & (Join-Path $fenceWork 'fenced-hooks-fixture.exe') $faultCase
    if($LASTEXITCODE -ne 0){throw "Fenced hooks case $faultCase failed"}
}
