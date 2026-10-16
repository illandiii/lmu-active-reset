param([string]$ZigPath, [string]$GamePath = 'D:\SteamLibrary\steamapps\common\Le Mans Ultimate')
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$ErrorActionPreference = 'Stop'
if (-not $ZigPath) {
    $ZigPath = Join-Path $projectRoot 'work\tools\zig-x86_64-windows-0.16.0\zig.exe'
}
$inventory = Get-Content -LiteralPath (Join-Path $projectRoot 'inventory.json') -Raw | ConvertFrom-Json
foreach ($entry in $inventory.files) {
    $actual = (Get-FileHash -LiteralPath (Join-Path $GamePath $entry.relative_path) -Algorithm SHA256).Hash
    if ($actual -ne $entry.sha256) { throw "Unsupported file/build: $($entry.relative_path)" }
}
$env:ZIG_GLOBAL_CACHE_DIR = [IO.Path]::GetFullPath((Join-Path $projectRoot 'work\zig-cache'))
$env:ZIG_LOCAL_CACHE_DIR = [IO.Path]::GetFullPath((Join-Path $projectRoot 'work\zig-local-cache'))
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'bin') | Out-Null
$buildLog = Join-Path $projectRoot 'work\build.log'
& $ZigPath c++ -std=c++17 -O2 -target x86_64-windows-gnu -I (Join-Path $GamePath 'Support\SharedMemoryInterface') (Join-Path $projectRoot 'src/native/telemetry/diagnostic.cpp') -o (Join-Path $projectRoot 'bin\diagnostic.exe') 2> $buildLog
if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $buildLog -Tail 30; throw 'C++ compilation failed' }
& (Join-Path $projectRoot 'bin\diagnostic.exe') --schema | Set-Content -LiteralPath (Join-Path $projectRoot 'schema.json')
if ($LASTEXITCODE -ne 0) { throw 'Schema inspection failed' }
