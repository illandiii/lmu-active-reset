#ifndef LMU_ACTIVE_RESET_HOTKEYS_WIN32_H
#define LMU_ACTIVE_RESET_HOTKEYS_WIN32_H
#include <windows.h>
extern "C" {
#include "active_reset_control.h"
}
struct ResetMailbox {
    SRWLOCK lock;
    ResetRequest request;
    bool pending;
};
struct ResetHotkeys {
    ResetKeys keys;
    DWORD target_pid;
    int save_vk, load_vk;
    uint64_t next_id;
};
bool reset_mailbox_post(ResetMailbox *, const ResetRequest &);
bool reset_mailbox_take(ResetMailbox *, ResetRequest &);
/* Zero-initialize before first use. Reconfiguration preserves command IDs and
 * disarms keys until release. Distinct F1..F24; no global input injection. */
bool reset_hotkeys_configure(ResetHotkeys *, DWORD pid, int save_vk, int load_vk);
int reset_hotkeys_poll(ResetHotkeys *, const ResetIdentity &, ResetMailbox *);
#endif
