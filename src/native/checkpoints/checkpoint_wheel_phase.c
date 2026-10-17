#include "checkpoint_wheel_phase.h"
#include <string.h>
static int valid(const WheelPhase p[4], const WheelDeformation deformation[4],
                 const RigidSavedBody *bodies, size_t n) {
    if (!p || !bodies || !n || n > 32)
        return 0;
    WheelSavedSpin spin[4];
    RigidSavedBody hubs[4], wheels[4];
    WheelPreparedSpin prepared[4];
    for (int w = 0; w < 4; w++) {
        if (p[w].hub_index >= n || p[w].wheel_index >= n || p[w].hub_index == p[w].wheel_index)
            return 0;
        for (int j = 0; j < w; j++)
            if (p[w].wheel_index == p[j].wheel_index)
                return 0;
        spin[w] = p[w].spin;
        hubs[w] = bodies[p[w].hub_index];
        wheels[w] = bodies[p[w].wheel_index];
    }
    return (deformation ? wheel_validate_deformed_state(spin, deformation, hubs, wheels)
                        : wheel_prepare_consistent_checkpoint(spin, hubs, wheels, prepared)) ==
           RIGID_PREPARE_OK;
}
int wheel_phase_valid(const WheelPhase p[4], const RigidSavedBody *bodies, size_t n) {
    return valid(p, NULL, bodies, n);
}
int wheel_phase_deformed_valid(const WheelPhase p[4], const WheelDeformation deformation[4],
                               const RigidSavedBody *bodies, size_t n) {
    return deformation && valid(p, deformation, bodies, n);
}
int wheel_phase_save(WheelPhaseBank *b, const PartialCheckpoint *s, const WheelPhase p[4]) {
    if (!b || !s || !s->sequence || !wheel_phase_valid(p, s->bodies, s->body_count))
        return 0;
    memcpy(b->phase, p, sizeof(b->phase));
    b->sequence = s->sequence;
    return 1;
}
int wheel_phase_prepare(const WheelPhaseBank *b, const PartialCheckpoint *s, const WheelPhase p[4],
                        const RigidSavedBody *current, size_t n, WheelPhase out[4]) {
    return wheel_phase_prepare_deformed(b, s, p, NULL, current, n, out);
}
int wheel_phase_prepare_deformed(const WheelPhaseBank *b, const PartialCheckpoint *s,
                                 const WheelPhase p[4], const WheelDeformation deformation[4],
                                 const RigidSavedBody *current, size_t n, WheelPhase out[4]) {
    if (!b || !s || !out || !b->sequence || b->sequence != s->sequence ||
        !wheel_phase_valid(b->phase, s->bodies, s->body_count) ||
        !valid(p, deformation, current, n))
        return 0;
    for (int w = 0; w < 4; w++)
        if (b->phase[w].hub_index != p[w].hub_index || b->phase[w].wheel_index != p[w].wheel_index)
            return 0;
    memcpy(out, b->phase, sizeof(b->phase));
    return 1;
}
int wheel_phase_plan(const WheelPhaseBank *b, const PartialCheckpoint *s,
                     const RestoreBackend *backend, RestorePlan *work, RestorePlan *out) {
    if (!b || !s || !backend || !backend->context || !backend->allowed || !backend->read || !work ||
        !out || work == out || !work->count || work->count > RESTORE_MAX_FIELDS - 4 ||
        b->sequence != s->sequence || !b->sequence ||
        !wheel_phase_valid(b->phase, s->bodies, s->body_count))
        return RESTORE_BAD_PLAN;
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    for (uint32_t w = 0; w < 4; w++) {
        uint32_t slot = RESTORE_WHEEL_BASE + w;
        for (size_t i = 0; i < work->count; i++) {
            const RestoreField *f = work->fields + i;
            if (f->object_slot == slot && f->field_offset < 0x130 &&
                0x120 < f->field_offset + f->size)
                return RESTORE_BAD_PLAN;
        }
        if (!backend->allowed(backend->user, slot, 0x120, 16))
            return RESTORE_FIELD_FORBIDDEN;
        RestoreField *f = work->fields + work->count;
        memset(f, 0, sizeof(*f));
        f->object_slot = slot;
        f->field_offset = 0x120;
        f->size = 16;
        if (!backend->read(backend->user, slot, 0x120, f->expected, 16))
            return RESTORE_READ_FAILED;
        double spin[2] = {b->phase[w].spin.angle, b->phase[w].spin.angular_rate};
        memcpy(f->desired, spin, 16);
        ++work->count;
    }
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    *out = *work;
    return 0;
}
