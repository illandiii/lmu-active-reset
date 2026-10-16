# Embedded dialogs keep the WPF dispatcher and the native host heartbeat running.
function Show-SectorModal([string]$Mode) {
    if ($ui.ModalLayer.Visibility -ne 'Visible') { $script:previousFocus = [Windows.Input.Keyboard]::FocusedElement }
    $script:modalMode = $Mode
    $ui.MainContent.IsEnabled = $false
    $ui.ModalLayer.Visibility = 'Visible'
    $ui.CaptureCard.Visibility = if ($Mode -eq 'close') { 'Collapsed' } else { 'Visible' }
    $ui.CloseCard.Visibility = if ($Mode -eq 'close') { 'Visible' } else { 'Collapsed' }
    $ui.CaptureTitle.Text = if ($Mode -eq 'save') { 'Save shortcut' } else { 'Restore shortcut' }
    $ui.CaptureHint.Text = 'Esc to cancel'
    if ($Mode -eq 'close') { $ui.KeepRunning.Focus() | Out-Null }
    else { $ui.CancelCapture.Focus() | Out-Null }
}

function Hide-SectorModal {
    $script:modalMode = $null
    $ui.ModalLayer.Visibility = 'Collapsed'
    $ui.MainContent.IsEnabled = $true
    if ($script:previousFocus) { $script:previousFocus.Focus() | Out-Null }
}

function Get-ShortcutLabel($Key) {
    if ($Key.name -like 'Oem*') {
        # OEM punctuation follows the active keyboard layout, not a US-only label.
        $character = [ResetWindowTheme]::MapVirtualKey($Key.virtual_key, 2) -band 0x7FFFFFFF
        if ($character -gt 0 -and $character -le 65535) { return [string][char]$character }
    }
    return $Key.label
}

function Accept-ShortcutKey([int]$VirtualKey, [bool]$HasModifiers = $false) {
    if ($script:modalMode -notin @('save', 'restore')) { return }
    $key = $script:resetKeyMap | Where-Object virtual_key -EQ $VirtualKey | Select-Object -First 1
    if ($HasModifiers -or $null -eq $key) {
        $ui.CaptureHint.Text = 'Choose a single key. Esc to cancel.'
        return
    }
    $target = if ($script:modalMode -eq 'save') { $ui.SaveKey } else { $ui.RestoreKey }
    $other = if ($script:modalMode -eq 'save') { $ui.RestoreKey } else { $ui.SaveKey }
    if ((Get-ResetKey $other.Tag).virtual_key -eq $VirtualKey) {
        $ui.CaptureHint.Text = 'This key is already assigned.'
        return
    }
    $previous = $target.Tag
    $target.Tag = $key.name
    if (-not (Save-Preferences)) {
        $target.Tag = $previous
        $ui.CaptureHint.Text = 'Could not save the shortcut.'
        return
    }
    $target.Content = Get-ShortcutLabel $key
    Hide-SectorModal
}

function Initialize-ResetInteractions {
    $script:modalMode = $null
    $script:previousFocus = $null
    $ui.SaveKey.Content = Get-ShortcutLabel (Get-ResetKey $ui.SaveKey.Tag)
    $ui.RestoreKey.Content = Get-ShortcutLabel (Get-ResetKey $ui.RestoreKey.Tag)
    $ui.SaveKey.Add_Click({ Show-SectorModal 'save' })
    $ui.RestoreKey.Add_Click({ Show-SectorModal 'restore' })
    $ui.CancelCapture.Add_Click({ Hide-SectorModal })
    $ui.CancelClose.Add_Click({ Hide-SectorModal })
    $ui.KeepRunning.Add_Click({ Hide-SectorModal; $window.Hide() })
    $ui.QuitSector.Add_Click({ Hide-SectorModal; Request-Exit })
    $ui.ResetTab.Add_Checked({ $ui.ResetPanel.Visibility = 'Visible'; $ui.OverlayPanel.Visibility = 'Collapsed' })
    $ui.OverlayTab.Add_Checked({ $ui.ResetPanel.Visibility = 'Collapsed'; $ui.OverlayPanel.Visibility = 'Visible' })
    $window.Add_PreviewKeyDown({
        param($sender, $event)
        if (-not $script:modalMode) { return }
        $key = if ($event.Key -eq 'System') { $event.SystemKey } else { $event.Key }
        if ($key -eq 'Escape') {
            $event.Handled = $true
            Hide-SectorModal
        } elseif ($script:modalMode -ne 'close') {
            $event.Handled = $true
            if (-not $event.IsRepeat) {
                $hasModifiers = [Windows.Input.Keyboard]::Modifiers -ne [Windows.Input.ModifierKeys]::None
                Accept-ShortcutKey ([Windows.Input.KeyInterop]::VirtualKeyFromKey($key)) $hasModifiers
            }
        }
    })
    $window.Add_Deactivated({
        if ($script:modalMode -in @('save', 'restore')) { Hide-SectorModal }
    })
}
