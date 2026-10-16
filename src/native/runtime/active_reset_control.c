#include "active_reset_control.h"
#include <string.h>
static int valid_identity(const ResetIdentity *i) {
    return i && i->process && i->session && i->vehicle && i->configuration;
}
int active_reset_key_poll(ResetKeys *keys, int focus, int save, int load, int modifiers) {
    if (!keys)
        return RESET_NONE;
    if (!focus || modifiers || (save && load)) {
        keys->armed = 0;
        return RESET_NONE;
    }
    if (!save && !load) {
        keys->armed = 1;
        return RESET_NONE;
    }
    if (!keys->armed)
        return RESET_NONE;
    keys->armed = 0;
    return save ? RESET_SAVE : RESET_LOAD;
}
int active_reset_process(ResetControl *state, uint64_t thread, uint64_t now,
                         const ResetIdentity *identity, int allowed, const ResetRequest *request,
                         const ResetActions *actions) {
    if (!state || !thread || thread != state->owner_thread)
        return CONTROL_WRONG_THREAD;
    if (state->busy)
        return CONTROL_BUSY;
    if (state->quarantined)
        return CONTROL_QUARANTINED;
    if (!valid_identity(identity)) {
        state->checkpoint_valid = 0;
        return CONTROL_REJECTED;
    }
    if (memcmp(identity, &state->identity, sizeof(*identity))) {
        state->identity = *identity;
        state->checkpoint_valid = 0;
    }
    if (!request)
        return CONTROL_NO_REQUEST;
    if (!request->id || request->id <= state->last_request_id)
        return CONTROL_REJECTED;
    /* Consume stale/forbidden requests so regaining focus cannot replay them. */
    state->last_request_id = request->id;
    if (!allowed || request->issued_ms > now || now - request->issued_ms > 1500 ||
        memcmp(&request->identity, identity, sizeof(*identity)) || !actions || !actions->capture ||
        !actions->restore)
        return CONTROL_REJECTED;
    if (request->action != RESET_SAVE && request->action != RESET_LOAD)
        return CONTROL_REJECTED;
    if (request->action == RESET_LOAD && !state->checkpoint_valid)
        return CONTROL_NO_CHECKPOINT;
    state->busy = 1;
    int result;
    if (request->action == RESET_SAVE) {
        result = actions->capture(actions->user, identity);
        if (!result) {
            state->checkpoint_valid = 1;
            ++state->saves;
            result = CONTROL_OK;
        } else
            result = CONTROL_SAVE_FAILED;
    } else {
        result = actions->restore(actions->user, identity);
        if (result == FLOW_OK) {
            ++state->loads;
            result = CONTROL_OK;
        } else if (result == FLOW_COMPENSATED)
            result = CONTROL_RESTORE_RECOVERED;
        else if (result == FLOW_QUARANTINED) {
            state->quarantined = 1;
            state->checkpoint_valid = 0;
            result = CONTROL_QUARANTINED;
        } else
            result = CONTROL_REJECTED;
    }
    state->busy = 0;
    return result;
}
