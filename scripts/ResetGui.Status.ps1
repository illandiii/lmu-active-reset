# Presentation only: native acknowledgements remain the source of checkpoint state.
function Get-ResetStatusView($Snapshot) {
    $view = [ordered]@{
        Title = 'Waiting for LMU'
        Detail = 'The app connects automatically when LMU starts.'
        Tone = 'Secondary'
        Checkpoint = 'No checkpoint'
        Summary = 'Save a point on track to return to its position and speed.'
        Message = [string]$Snapshot.message
    }
    if ($Snapshot.checkpoint) {
        $view.Checkpoint = 'Checkpoint saved'
        $parts = @()
        if ($null -ne $Snapshot.saved_speed_kmh) {
            $parts += ('{0:0} km/h saved' -f $Snapshot.saved_speed_kmh)
        }
        $count = [int]$Snapshot.restore_count
        $parts += if ($count -eq 1) { '1 restore' } else { "$count restores" }
        $view.Summary = $parts -join '  /  '
    }
    switch ($Snapshot.phase) {
        'waiting' { $view.Title = 'Waiting for driving'; $view.Detail = 'LMU detected. Enter offline Practice.' }
        'starting' { $view.Title = 'Connecting'; $view.Detail = 'Checking the game and vehicle.' }
        'active' {
            $view.Title = 'Connected'
            $view.Detail = 'Your shortcuts are ready in LMU.'
            $view.Tone = 'Accent'
        }
        'paused' {
            $view.Title = 'Waiting for driving'
            $view.Detail = if ($Snapshot.checkpoint) { 'Checkpoint retained while driving is paused.' } else { 'Enter offline Practice to save a checkpoint.' }
        }
        'stopping' { $view.Title = 'Disconnecting'; $view.Detail = 'Finishing the current operation.' }
        'stopped' { $view.Title = 'Disconnected'; $view.Detail = 'Open Sector again to reconnect.' }
        'error' { $view.Title = 'Connection stopped'; $view.Detail = [string]$Snapshot.message; $view.Tone = 'Signal' }
    }
    return [pscustomobject]$view
}
