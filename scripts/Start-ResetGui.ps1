param([switch]$Preview, [string]$PythonPath)
$ErrorActionPreference = 'Stop'
# Node launched from PowerShell 7 can pass its incompatible module search path
# to Windows PowerShell. Prefer modules belonging to the current interpreter.
$env:PSModulePath = (Join-Path $PSHOME 'Modules') + [IO.Path]::PathSeparator + $env:PSModulePath
$projectRoot = Split-Path -Parent $PSScriptRoot
$desktop = Join-Path $projectRoot 'src/ui/electron'
$logDirectory = Join-Path $projectRoot 'logs'
[IO.Directory]::CreateDirectory($logDirectory) | Out-Null
$startupLog = Join-Path $logDirectory 'desktop-startup.log'
[IO.File]::WriteAllText($startupLog, '', [Text.UTF8Encoding]::new($false))
try {
    $node = (Get-Command node.exe -ErrorAction Stop).Source
    $npm = (Get-Command npm.cmd -ErrorAction Stop).Source
    if ($PythonPath) { $env:SECTOR_PYTHON = $PythonPath }
    Remove-Item Env:ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue
    Push-Location $desktop
    try {
        $lockHash = (Get-FileHash -LiteralPath (Join-Path $desktop 'package-lock.json') -Algorithm SHA256).Hash
        $stamp = Join-Path $desktop 'node_modules/.sector-lock'
        $installedHash = if (Test-Path -LiteralPath $stamp) { [IO.File]::ReadAllText($stamp) } else { '' }
        if ($installedHash -ne $lockHash -or -not (Test-Path -LiteralPath (Join-Path $desktop 'node_modules/vite/bin/vite.js'))) {
            & $npm ci 2>&1 | Out-File -FilePath $startupLog -Encoding utf8 -Append
            if ($LASTEXITCODE -ne 0) { throw "Dependency installation failed. See $startupLog" }
        }
        $electron = Join-Path $desktop 'node_modules/electron/dist/electron.exe'
        if (-not (Test-Path -LiteralPath $electron)) {
            & $node (Join-Path $desktop 'node_modules/electron/install.js') 2>&1 | Out-File -FilePath $startupLog -Encoding utf8 -Append
            if ($LASTEXITCODE -ne 0) { throw "Electron setup failed. See $startupLog" }
        }
        # Rebuild the small local renderer so a source update cannot launch stale UI.
        & $npm run build 2>&1 | Out-File -FilePath $startupLog -Encoding utf8 -Append
        if ($LASTEXITCODE -ne 0) { throw "Desktop build failed. See $startupLog" }
        [IO.File]::WriteAllText($stamp, $lockHash)
        $arguments = '"' + $desktop + '"'
        if ($Preview) { $arguments += ' --preview' }
        Start-Process -FilePath $electron -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden
    } finally { Pop-Location }
} catch {
    [IO.File]::AppendAllText($startupLog, "`nStartup error: $($_.Exception.Message)`n")
    # Only bootstrap failures use a system dialog; app decisions stay in Electron.
    Add-Type -AssemblyName PresentationFramework
    [Windows.MessageBox]::Show("Sector could not start.`n`n$($_.Exception.Message)`n`nNode.js 22.12+ and npm are required.", 'Sector', 'OK', 'Error') | Out-Null
    exit 1
}
