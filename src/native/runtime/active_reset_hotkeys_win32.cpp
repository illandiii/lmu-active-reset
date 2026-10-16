#include "active_reset_hotkeys_win32.h"
bool reset_mailbox_post(ResetMailbox *mailbox, const ResetRequest &request) {
    if (!mailbox || !TryAcquireSRWLockExclusive(&mailbox->lock))
        return false;
    bool accepted = !mailbox->pending;
    if (accepted) {
        mailbox->request = request;
        mailbox->pending = true;
    }
    ReleaseSRWLockExclusive(&mailbox->lock);
    return accepted;
}
bool reset_mailbox_take(ResetMailbox *mailbox, ResetRequest &request) {
    if (!mailbox || !TryAcquireSRWLockExclusive(&mailbox->lock))
        return false;
    bool present = mailbox->pending;
    if (present) {
        request = mailbox->request;
        mailbox->pending = false;
    }
    ReleaseSRWLockExclusive(&mailbox->lock);
    return present;
}
bool reset_hotkeys_configure(ResetHotkeys *keys, DWORD pid, int save_vk, int load_vk) {
    if (!keys || !pid || save_vk < VK_F1 || save_vk > VK_F24 || load_vk < VK_F1 ||
        load_vk > VK_F24 || save_vk == load_vk)
        return false;
    const uint64_t sequence = keys->next_id;
    *keys = ResetHotkeys{};
    keys->next_id = sequence;
    keys->target_pid = pid;
    keys->save_vk = save_vk;
    keys->load_vk = load_vk;
    return true;
}
int reset_hotkeys_poll(ResetHotkeys *keys, const ResetIdentity &identity, ResetMailbox *mailbox) {
    if (!keys || !mailbox)
        return RESET_NONE;
    DWORD focused = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &focused);
    bool modifiers = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
                     (GetAsyncKeyState(VK_MENU) & 0x8000) ||
                     (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
                     (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);
    int action = active_reset_key_poll(&keys->keys, focused == keys->target_pid,
                                       GetAsyncKeyState(keys->save_vk) & 0x8000,
                                       GetAsyncKeyState(keys->load_vk) & 0x8000, modifiers);
    if (!action)
        return RESET_NONE;
    if (keys->next_id == UINT64_MAX)
        return RESET_NONE;
    ResetRequest request{++keys->next_id, GetTickCount64(), identity, (uint32_t)action};
    return reset_mailbox_post(mailbox, request) ? action : RESET_NONE;
}
