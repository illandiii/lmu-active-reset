#ifndef LMU_ACTIVE_RESET_CONTROL_H
#define LMU_ACTIVE_RESET_CONTROL_H
#include <stdint.h>
#include "restore_flow.h"
enum { RESET_NONE, RESET_SAVE, RESET_LOAD };
typedef struct ResetIdentity {
    uint64_t process, session, vehicle, configuration;
} ResetIdentity;
typedef struct ResetRequest {
    uint64_t id, issued_ms;
    ResetIdentity identity;
    uint32_t action;
} ResetRequest;
typedef struct ResetControl {
    uint64_t owner_thread, last_request_id;
    ResetIdentity identity;
    int checkpoint_valid, busy, quarantined;
    uint64_t saves, loads;
} ResetControl;
typedef struct ResetActions {
    void *user;
    int (*capture)(void *, const ResetIdentity *);
    int (*restore)(void *, const ResetIdentity *);
} ResetActions;
enum {
    CONTROL_OK,
    CONTROL_NO_REQUEST,
    CONTROL_REJECTED,
    CONTROL_NO_CHECKPOINT,
    CONTROL_SAVE_FAILED,
    CONTROL_RESTORE_RECOVERED,
    CONTROL_QUARANTINED,
    CONTROL_WRONG_THREAD,
    CONTROL_BUSY
};
/* Call every owner-thread boundary, even with request=NULL, to invalidate a
 * checkpoint when identity changes. Callbacks run synchronously on this thread.
 * capture must publish a replacement atomically and retain the old one on failure. */
int active_reset_process(ResetControl *, uint64_t thread, uint64_t now_ms, const ResetIdentity *,
                         int context_allowed, const ResetRequest *, const ResetActions *);
typedef struct ResetKeys {
    int armed;
} ResetKeys;
/* Returns an edge only after both keys have been released in the target window.
 * Losing focus, modifiers or simultaneous keys disarms until a clean release. */
int active_reset_key_poll(ResetKeys *, int focused, int save_down, int load_down,
                          int modifiers_down);
#endif
