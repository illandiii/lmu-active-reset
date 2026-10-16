# Isolated window smoke checks; only loaded in Preview mode.
function Invoke-ResetGuiSmokeStep {
    switch ($script:smokeStep) {
        0 {
            if ((Show-CloseChoice 'background') -ne 'background') { throw 'Background choice failed.' }
            $window.Hide()
            if ($window.IsVisible) { throw 'Window did not hide.' }
        }
        1 {
            Show-SectorWindow
            if (-not $window.IsVisible) { throw 'Window did not reopen.' }
            $script:sessionPath = Join-Path $projectRoot ('work/gui-smoke-' + [guid]::NewGuid().ToString('N'))
            [IO.Directory]::CreateDirectory($script:sessionPath) | Out-Null
            [IO.File]::WriteAllText((Join-Path $script:sessionPath 'status.json'), '{"sequence":1,"phase":"active","checkpoint":true,"saved_speed_kmh":80,"restore_count":2,"message":"Checkpoint applied."}')
            Read-SessionStatus
            if ($ui.CheckpointSummary.Text -ne '80 km/h saved  /  2 restores' -or $ui.StatusTitle.Text -ne 'Connected') { throw 'Controller status did not reach the window.' }
            # An active child must receive a cooperative stop before the window exits.
            $script:child = [pscustomobject]@{ HasExited = $false }
            Request-Exit
            if ($script:canClose -or -not [IO.File]::Exists((Join-Path $script:sessionPath 'stop'))) {
                throw 'Quit did not wait for cooperative host shutdown.'
            }
            $script:child = $null
            $script:closingRequested = $false
        }
        2 {
            if ((Show-CloseChoice 'quit') -ne 'quit') { throw 'Quit choice failed.' }
            Request-Exit
        }
    }
    $script:smokeStep++
}
