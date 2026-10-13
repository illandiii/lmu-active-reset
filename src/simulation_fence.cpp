#include "simulation_fence.h"
#include <cstring>
void simulation_fence_init(SimulationFence *f, uint64_t epoch, DWORD thread,
                           const uintptr_t *objects, const uintptr_t *contacts,
                           unsigned contact_count) {
    // Initialization only before hooks are active; never a quarantine reset API.
    if (!f)
        return;
    memset(f, 0, sizeof(*f));
    InitializeSRWLock(&f->lock);
    f->epoch = epoch;
    f->owner_thread = thread;
    if (!objects || !objects[FENCE_BOUNDARY] || !objects[FENCE_SCORING]) {
        f->violated = true;
        f->mode = FENCE_QUARANTINED;
        return;
    }
    memcpy(f->object, objects, sizeof(f->object));
    if (!contacts && !contact_count) {
        contacts = objects + FENCE_CONTACTS;
        contact_count = 1;
    }
    bool valid = epoch && thread && contacts && contact_count && contact_count <= 4;
    if (valid)
        for (unsigned i = 0; i < contact_count; i++) {
            if (!contacts[i])
                valid = false;
            for (unsigned j = 0; j < i; j++)
                if (contacts[i] == contacts[j])
                    valid = false;
        }
    if (!valid) {
        f->violated = true;
        f->mode = FENCE_QUARANTINED;
        return;
    }
    memcpy(f->contact_objects, contacts, contact_count * sizeof(*contacts));
    f->contact_count = contact_count;
}
bool simulation_fence_callback_enter(SimulationFence *f, unsigned phase, uintptr_t object,
                                     FenceTicket *t) {
    if (t)
        *t = {};
    if (!f || !t || phase >= FENCE_PHASES)
        return false;
    AcquireSRWLockExclusive(&f->lock);
    if (!f->contact_count) {
        ++f->blocked[phase];
        ReleaseSRWLockExclusive(&f->lock);
        return false;
    }
    bool selected = object == f->object[phase];
    if (phase == FENCE_CONTACTS) {
        selected = false;
        for (unsigned i = 0; i < f->contact_count; i++)
            if (object == f->contact_objects[i])
                selected = true;
    }
    if (!selected) {
        t->admitted = true;
        ReleaseSRWLockExclusive(&f->lock);
        return true;
    }
    if (f->mode != FENCE_RUNNING) {
        ++f->blocked[phase];
        if (f->mode == FENCE_HELD)
            f->violated = true;
        ReleaseSRWLockExclusive(&f->lock);
        return false;
    }
    if (GetCurrentThreadId() != f->owner_thread) {
        f->violated = true;
        ++f->blocked[phase];
        ReleaseSRWLockExclusive(&f->lock);
        return false;
    }
    ++f->active[phase];
    *t = {f, phase, true};
    ReleaseSRWLockExclusive(&f->lock);
    return true;
}
void simulation_fence_callback_leave(FenceTicket *t) {
    if (!t || !t->admitted)
        return;
    if (!t->fence) {
        t->admitted = false;
        return;
    }
    auto *f = t->fence;
    AcquireSRWLockExclusive(&f->lock);
    if (f->active[t->phase])
        --f->active[t->phase];
    else
        f->violated = true;
    t->admitted = false;
    ReleaseSRWLockExclusive(&f->lock);
}
bool simulation_fence_may_advance(const FenceTicket *t) {
    if (!t || !t->admitted)
        return false;
    if (!t->fence)
        return true;
    auto *f = t->fence;
    AcquireSRWLockShared(&f->lock);
    bool valid = f->mode == FENCE_RUNNING && !f->violated;
    ReleaseSRWLockShared(&f->lock);
    return valid;
}
bool simulation_fence_hold(SimulationFence *f, uint64_t epoch) {
    if (!f)
        return false;
    AcquireSRWLockExclusive(&f->lock);
    bool valid = epoch && epoch == f->epoch && GetCurrentThreadId() == f->owner_thread &&
                 f->mode == FENCE_RUNNING && !f->violated && f->active[FENCE_BOUNDARY] == 1 &&
                 !f->active[FENCE_SCORING] && !f->active[FENCE_CONTACTS];
    if (valid) {
        f->mode = FENCE_HELD;
        f->dirty = false;
    }
    ReleaseSRWLockExclusive(&f->lock);
    return valid;
}
bool simulation_fence_owner_alive(SimulationFence *f, uint64_t epoch) {
    if (!f)
        return false;
    AcquireSRWLockShared(&f->lock);
    bool valid = epoch && epoch == f->epoch && GetCurrentThreadId() == f->owner_thread &&
                 f->mode == FENCE_HELD && !f->violated && f->active[FENCE_BOUNDARY] == 1 &&
                 !f->active[FENCE_SCORING] && !f->active[FENCE_CONTACTS];
    ReleaseSRWLockShared(&f->lock);
    return valid;
}
void simulation_fence_mark_dirty(SimulationFence *f) {
    AcquireSRWLockExclusive(&f->lock);
    if (f->mode == FENCE_HELD && GetCurrentThreadId() == f->owner_thread)
        f->dirty = true;
    else {
        f->violated = true;
        f->mode = FENCE_QUARANTINED;
    }
    ReleaseSRWLockExclusive(&f->lock);
}
void simulation_fence_release(SimulationFence *f, bool verified_resume) {
    AcquireSRWLockExclusive(&f->lock);
    if (f->mode == FENCE_HELD) {
        if (verified_resume && !f->violated && GetCurrentThreadId() == f->owner_thread) {
            f->mode = FENCE_RUNNING;
            f->dirty = false;
        } else
            f->mode = FENCE_QUARANTINED;
    }
    ReleaseSRWLockExclusive(&f->lock);
}
void simulation_fence_quarantine(SimulationFence *f) {
    AcquireSRWLockExclusive(&f->lock);
    f->mode = FENCE_QUARANTINED;
    ReleaseSRWLockExclusive(&f->lock);
}
bool simulation_fence_stop(SimulationFence *f) {
    if (!f)
        return false;
    AcquireSRWLockExclusive(&f->lock);
    bool valid = f->mode != FENCE_HELD && f->mode != FENCE_QUARANTINED && !f->dirty;
    if (valid) {
        f->mode = FENCE_STOPPING;
        for (unsigned n : f->active)
            if (n)
                valid = false;
    }
    ReleaseSRWLockExclusive(&f->lock);
    return valid;
}
