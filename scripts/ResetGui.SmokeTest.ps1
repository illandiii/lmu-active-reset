# Isolated interaction checks; only loaded in Preview mode.
function Assert-WindowState([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Send-CapturedKey([Windows.Input.Key]$Key) {
    $source = [Windows.PresentationSource]::FromVisual($window)
    $event = [Windows.Input.KeyEventArgs]::new([Windows.Input.Keyboard]::PrimaryDevice, $source, 0, $Key)
    $event.RoutedEvent = [Windows.Input.Keyboard]::PreviewKeyDownEvent
    $ui.CancelCapture.RaiseEvent($event)
    Assert-WindowState $event.Handled 'Capture did not consume the key event.'
}

function Find-VisualChild($Root, [type]$Type) {
    for ($index = 0; $index -lt [Windows.Media.VisualTreeHelper]::GetChildrenCount($Root); $index++) {
        $child = [Windows.Media.VisualTreeHelper]::GetChild($Root, $index)
        if ($child -is $Type) { return $child }
        $match = Find-VisualChild $child $Type
        if ($match) { return $match }
    }
}

function Invoke-ResetGuiSmokeStep {
    switch ($script:smokeStep) {
        0 {
            $ui.OverlayTab.IsChecked = $true
            Assert-WindowState ($ui.OverlayPanel.Visibility -eq 'Visible' -and $ui.ResetPanel.Visibility -eq 'Collapsed') 'Overlay tab failed.'
            $ui.ResetTab.IsChecked = $true
            Assert-WindowState ($ui.ResetPanel.Visibility -eq 'Visible') 'Reset tab failed.'
            $ui.SaveKey.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent))
            Assert-WindowState ($ui.ModalLayer.Visibility -eq 'Visible' -and -not $ui.MainContent.IsEnabled) 'Capture must be modal inside the app.'
            Send-CapturedKey 'G'
            Assert-WindowState ($ui.SaveKey.Tag -eq 'G' -and $script:preferences.save_key -eq 'G') 'Captured key was not applied.'
            Show-SectorModal 'restore'
            Send-CapturedKey 'G'
            Assert-WindowState ($script:modalMode -eq 'restore' -and $ui.CaptureHint.Text -eq 'This key is already assigned.') 'Duplicate key was accepted.'
            Accept-ShortcutKey 0x48 $true
            Assert-WindowState ($script:modalMode -eq 'restore') 'Modifier combination was accepted.'
            Send-CapturedKey 'Escape'
            Assert-WindowState ($ui.ModalLayer.Visibility -eq 'Collapsed') 'Escape did not cancel capture.'
            Show-SectorModal 'restore'
            Send-CapturedKey 'Space'
            Assert-WindowState ($script:preferences.restore_key -eq 'Space') 'Space capture failed.'
            $window.Height = 520
            $window.UpdateLayout()
        }
        1 {
            Assert-WindowState ($ui.ResetPanel.ScrollableHeight -gt 0) 'Compact layout did not scroll.'
            $ui.ResetPanel.ScrollToBottom()
            $window.UpdateLayout()
            $bar = Find-VisualChild $ui.ResetPanel ([Windows.Controls.Primitives.ScrollBar])
            Assert-WindowState ($bar.Width -eq 10 -and $bar.Value -gt 0) 'Custom scrollbar did not follow content.'
            $track = $bar.Template.FindName('PART_Track', $bar)
            Assert-WindowState ($null -ne $track -and [Math]::Abs($track.Value - $ui.ResetPanel.VerticalOffset) -lt 1) 'Scrollbar track is out of sync.'
            $window.Close()
            Assert-WindowState ($script:modalMode -eq 'close' -and $window.IsVisible) 'Close must open an embedded prompt.'
            $ui.CancelClose.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent))
            Assert-WindowState ($ui.MainContent.IsEnabled) 'Cancel did not restore interaction.'
            $window.Close()
            $ui.KeepRunning.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent))
            Assert-WindowState (-not $window.IsVisible) 'Background choice did not hide the window.'
        }
        2 {
            Show-SectorWindow
            Assert-WindowState $window.IsVisible 'Window did not reopen.'
            $script:sessionPath = Join-Path $projectRoot ('work/gui-smoke-' + [guid]::NewGuid().ToString('N'))
            [IO.Directory]::CreateDirectory($script:sessionPath) | Out-Null
            [IO.File]::WriteAllText((Join-Path $script:sessionPath 'status.json'), '{"sequence":1,"phase":"active","checkpoint":true,"saved_speed_kmh":80,"restore_count":2,"message":"Checkpoint applied."}')
            Read-SessionStatus
            Assert-WindowState ($ui.CheckpointSummary.Text -eq '80 km/h saved  /  2 restores' -and $ui.StatusTitle.Text -eq 'Connected') 'Controller status did not reach the window.'
            $script:child = [pscustomobject]@{ HasExited = $false }
            Request-Exit
            Assert-WindowState (-not $script:canClose -and [IO.File]::Exists((Join-Path $script:sessionPath 'stop'))) 'Quit did not wait for cooperative shutdown.'
            $script:child = $null
            $script:closingRequested = $false
        }
        3 {
            $window.Close()
            $ui.QuitSector.RaiseEvent([Windows.RoutedEventArgs]::new([Windows.Controls.Button]::ClickEvent))
        }
    }
    $script:smokeStep++
}
