param([Parameter(Mandatory=$true)][string]$ZigPath)
$projectRoot = Split-Path -Parent $PSScriptRoot

$ErrorActionPreference='Stop'
$warmWork=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))
$warmSrc=Join-Path $projectRoot 'src'
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $warmWork 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR=Join-Path $warmWork 'zig-local-cache'
& $ZigPath cc -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu "$warmSrc\rigid_restore_core.c" "$warmSrc\bar_constraint_core.c" "$warmSrc\constraint_warm_group.c" "$warmSrc\constraint_warm_fixture.c" -o "$warmWork\constraint-warm-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Warm group build failed'}
& "$warmWork\constraint-warm-fixture.exe"
if($LASTEXITCODE -ne 0){throw 'Warm group fixture failed'}
& $ZigPath cc -O2 -Wall -Wextra -Werror -target x86_64-windows-gnu -shared "$warmSrc\rigid_restore_core.c" "$warmSrc\bar_constraint_core.c" "$warmSrc\constraint_warm_group.c" "$warmSrc\constraint_warm_bridge.c" -o "$warmWork\constraint-warm-fixture.dll"
if($LASTEXITCODE -ne 0){throw 'Warm group bridge failed'}
