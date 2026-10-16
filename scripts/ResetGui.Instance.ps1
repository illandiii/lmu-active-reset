# The named event also lets a second launcher reveal a hidden primary window.
function Open-ResetGuiInstance([string]$Name = 'Local\Sector.Desktop') {
    $request = [Threading.EventWaitHandle]::new($false, [Threading.EventResetMode]::AutoReset, "$Name.ShowWindow")
    $mutex = [Threading.Mutex]::new($false, $Name)
    try { $owner = $mutex.WaitOne(0) }
    catch [Threading.AbandonedMutexException] { $owner = $true }
    if (-not $owner) { $request.Set() | Out-Null }
    return [pscustomobject]@{ Mutex = $mutex; ShowRequest = $request; IsOwner = $owner }
}

function Close-ResetGuiInstance($Instance) {
    if ($null -eq $Instance) { return }
    if ($Instance.IsOwner) { $Instance.Mutex.ReleaseMutex() }
    $Instance.Mutex.Dispose()
    $Instance.ShowRequest.Dispose()
}
