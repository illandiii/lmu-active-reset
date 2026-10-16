$ErrorActionPreference = 'Stop'
$desktop = Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'src/ui/electron'
Push-Location $desktop
try {
    & npm.cmd ci
    if ($LASTEXITCODE -ne 0) { throw 'Desktop dependency installation failed.' }
    & node.exe (Join-Path $desktop 'node_modules/electron/install.js')
    if ($LASTEXITCODE -ne 0) { throw 'Electron runtime setup failed.' }
    & npm.cmd run build
    if ($LASTEXITCODE -ne 0) { throw 'Desktop renderer build failed.' }
    [IO.File]::WriteAllText((Join-Path $desktop 'node_modules/.sector-lock'), (Get-FileHash -LiteralPath (Join-Path $desktop 'package-lock.json') -Algorithm SHA256).Hash)
} finally { Pop-Location }
