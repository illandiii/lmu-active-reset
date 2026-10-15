function Test-ResetSettings($Settings) {
    if ($null -eq $Settings) { return $false }
    if ($Settings.save_key -isnot [string] -or $Settings.restore_key -isnot [string]) { return $false }
    if ($Settings.sound -isnot [bool]) { return $false }
    $pattern = '^F([1-9]|1[0-9]|2[0-4])$'
    return ($Settings.save_key -cmatch $pattern -and $Settings.restore_key -cmatch $pattern -and
        $Settings.save_key -cne $Settings.restore_key)
}

function Read-ResetSettings([string]$Path) {
    $defaults = [pscustomobject]@{ save_key = 'F6'; restore_key = 'F7'; sound = $true }
    if (-not [IO.File]::Exists($Path)) { return $defaults }
    try {
        $value = [IO.File]::ReadAllText($Path) | ConvertFrom-Json
        if (Test-ResetSettings $value) { return $value }
    } catch { }
    return $defaults
}

function Save-ResetSettings([string]$Path, $Settings) {
    if (-not (Test-ResetSettings $Settings)) { throw 'Choose two different function keys (F1-F24).' }
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    $temporary = $Path + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllText($temporary, ($Settings | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
        if ([IO.File]::Exists($Path)) { [IO.File]::Replace($temporary, $Path, [NullString]::Value) }
        else { [IO.File]::Move($temporary, $Path) }
    } finally {
        if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
    }
}
