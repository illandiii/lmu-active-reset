#include "../../src/native/runtime/active_reset_control.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
static int captures, restores, save_failure, flow_result;
static int capture(void *p, const ResetIdentity *i) {
    (void)p;
    (void)i;
    ++captures;
    return save_failure;
}
static int restore(void *p, const ResetIdentity *i) {
    (void)p;
    (void)i;
    ++restores;
    return flow_result;
}
int main(void) {
    ResetKeys keys = {0};
    assert(!active_reset_key_poll(&keys, 1, 1, 0, 0));
    assert(!active_reset_key_poll(&keys, 1, 0, 0, 0));
    assert(active_reset_key_poll(&keys, 1, 1, 0, 0) == RESET_SAVE);
    for (int i = 0; i < 100; i++)
        assert(!active_reset_key_poll(&keys, 1, 1, 0, 0));
    assert(!active_reset_key_poll(&keys, 0, 0, 0, 0));
    assert(!active_reset_key_poll(&keys, 1, 0, 1, 0));
    assert(!active_reset_key_poll(&keys, 1, 0, 0, 0));
    assert(active_reset_key_poll(&keys, 1, 0, 1, 0) == RESET_LOAD);
    assert(!active_reset_key_poll(&keys, 1, 0, 0, 0));
    assert(!active_reset_key_poll(&keys, 1, 1, 1, 0));
    assert(!active_reset_key_poll(&keys, 1, 1, 0, 0));
    assert(!active_reset_key_poll(&keys, 1, 0, 0, 0));
    assert(!active_reset_key_poll(&keys, 1, 1, 0, 1));
    assert(!active_reset_key_poll(&keys, 1, 1, 0, 0));
    ResetIdentity id = {1, 2, 3, 4};
    ResetControl state = {0};
    state.owner_thread = 7;
    ResetActions actions = {0, capture, restore};
    ResetRequest request = {1, 100, id, RESET_LOAD};
    assert(active_reset_process(&state, 8, 100, &id, 1, &request, &actions) ==
           CONTROL_WRONG_THREAD);
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) ==
           CONTROL_NO_CHECKPOINT);
    request.id++;
    request.action = RESET_SAVE;
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) == CONTROL_OK &&
           state.checkpoint_valid && captures == 1);
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) == CONTROL_REJECTED &&
           captures == 1);
    request.id++;
    save_failure = 1;
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) ==
               CONTROL_SAVE_FAILED &&
           state.checkpoint_valid);
    save_failure = 0;
    request.id++;
    request.action = RESET_LOAD;
    assert(active_reset_process(&state, 7, 100, &id, 0, &request, &actions) == CONTROL_REJECTED &&
           !restores);
    request.id++;
    assert(active_reset_process(&state, 7, 2000, &id, 1, &request, &actions) == CONTROL_REJECTED &&
           !restores);
    request.id++;
    request.issued_ms = 101;
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) == CONTROL_REJECTED);
    request.id++;
    request.issued_ms = 100;
    // A fresh load request can use yesterday's checkpoint; only requests expire.
    request.issued_ms = 86400100;
    assert(active_reset_process(&state, 7, 86400100, &id, 1, &request, &actions) == CONTROL_OK &&
           restores == 1);
    request.issued_ms = 100;
    request.id++;
    flow_result = FLOW_COMPENSATED;
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) ==
               CONTROL_RESTORE_RECOVERED &&
           state.checkpoint_valid);
    id.session++;
    assert(active_reset_process(&state, 7, 100, &id, 1, NULL, &actions) == CONTROL_NO_REQUEST &&
           !state.checkpoint_valid);
    request.id++;
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) == CONTROL_REJECTED);
    request.id++;
    request.identity = id;
    request.action = RESET_SAVE;
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) == CONTROL_OK);
    request.id++;
    request.action = RESET_LOAD;
    flow_result = FLOW_QUARANTINED;
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) ==
               CONTROL_QUARANTINED &&
           !state.checkpoint_valid);
    request.id++;
    assert(active_reset_process(&state, 7, 100, &id, 1, &request, &actions) == CONTROL_QUARANTINED);
    puts("PASS controls: key edges/focus/modifiers/chords, owner thread, TTL, duplicate requests, "
         "failed-save preservation, session invalidation and fatal lockout.");
    return 0;
}
