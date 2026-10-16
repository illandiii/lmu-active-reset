param(
    [string]$ClangFormat = 'clang-format',
    [string]$Ruff = 'ruff'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$nativeRoots = @((Join-Path $projectRoot 'src/native'), (Join-Path $projectRoot 'tests/native'))
$nativeFiles = Get-ChildItem -LiteralPath $nativeRoots -File -Recurse |
    Where-Object Extension -In '.c', '.cpp', '.h'

foreach ($file in $nativeFiles) {
    & $ClangFormat -i --style=file $file.FullName
    if ($LASTEXITCODE -ne 0) { throw "Formatting failed: $($file.Name)" }
}

& $Ruff format (Join-Path $projectRoot 'src/python') (Join-Path $projectRoot 'tests/python') (Join-Path $projectRoot 'scripts/sector.py')
if ($LASTEXITCODE -ne 0) { throw 'Python formatting failed' }
