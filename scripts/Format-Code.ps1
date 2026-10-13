param(
    [string]$ClangFormat = 'clang-format',
    [string]$Ruff = 'ruff'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $projectRoot 'src'
$nativeFiles = Get-ChildItem -LiteralPath $sourceRoot -File |
    Where-Object Extension -In '.c', '.cpp', '.h'

foreach ($file in $nativeFiles) {
    & $ClangFormat -i --style=file $file.FullName
    if ($LASTEXITCODE -ne 0) { throw "Formatting failed: $($file.Name)" }
}

& $Ruff format $sourceRoot
if ($LASTEXITCODE -ne 0) { throw 'Python formatting failed' }
