#include "graphics_lifetime.h"
#include <limits>

void graphics_lifetime_init(GraphicsLifetime *state) {
    InitializeCriticalSection(&state->lock);
    state->generation = 1;
    state->active = 0;
    state->pin_thread = 0;
}
void graphics_operation_enter(GraphicsLifetime *state, bool reconstruct) {
    EnterCriticalSection(&state->lock);
    ++state->active;
    if (reconstruct && state->generation) {
        state->generation =
            state->generation == std::numeric_limits<uint64_t>::max() ? 0 : state->generation + 1;
    }
    LeaveCriticalSection(&state->lock);
}
void graphics_operation_leave(GraphicsLifetime *state) {
    EnterCriticalSection(&state->lock);
    if (state->active)
        --state->active;
    else
        state->generation = 0;
    LeaveCriticalSection(&state->lock);
}
bool graphics_try_pin(GraphicsLifetime *state, uint64_t *generation) {
    if (!state || !generation || !TryEnterCriticalSection(&state->lock))
        return false;
    if (state->active || !state->generation || state->pin_thread) {
        LeaveCriticalSection(&state->lock);
        return false;
    }
    state->pin_thread = GetCurrentThreadId();
    *generation = state->generation;
    return true;
}
bool graphics_pin_valid(GraphicsLifetime *state, uint64_t generation) {
    return state && state->pin_thread == GetCurrentThreadId() && generation &&
           state->generation == generation && !state->active;
}
void graphics_release_pin(GraphicsLifetime *state) {
    if (state && state->pin_thread == GetCurrentThreadId()) {
        state->pin_thread = 0;
        LeaveCriticalSection(&state->lock);
    }
}
