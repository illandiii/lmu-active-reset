#include "checkpoint_native_cache.h"
#include <math.h>
#include <float.h>
#include <string.h>
static int reciprocal(double a, double inverse) {
    return isfinite(a) && a > 0 && isfinite(inverse) && inverse > 0 &&
           fabs(1 / a - inverse) <= 32 * DBL_EPSILON * fabs(inverse);
}
int native_cache_valid(const NativeCache *c, const RigidSavedBody *b, size_t n) {
    if (!c || !b || !n || n > 32)
        return 0;
    for (size_t i = 0; i < n; i++) {
        const NativeBodyCache *v = c->bodies + i;
        if (!reciprocal(v->mass, b[i].mass.inverse_mass))
            return 0;
        for (int j = 0; j < 3; j++)
            if (!reciprocal(v->inertia_local[j], b[i].mass.inverse_inertia_local[j]) ||
                !isfinite(v->omega[j]))
                return 0;
        for (int j = 0; j < 6; j++)
            if (!isfinite(v->inverse_world[j]))
                return 0;
    }
    for (int w = 0; w < 4; w++)
        for (int s = 0; s < 8; s++) {
            for (int j = 0; j < 3; j++)
                if (!isfinite(c->sectors[w][s].first_moment[j]))
                    return 0;
            for (int j = 0; j < 2; j++)
                if (!isfinite(c->sectors[w][s].aggregate[j]))
                    return 0;
        }
    return 1;
}
static int replace(RestorePlan *p, uint32_t slot, uint32_t offset, const void *data,
                   uint32_t size) {
    size_t matches = 0;
    for (size_t i = 0; i < p->count; i++) {
        RestoreField *f = p->fields + i;
        if (f->object_slot == slot && f->field_offset == offset) {
            if (f->size != size)
                return 0;
            memcpy(f->desired, data, size);
            ++matches;
        }
    }
    return matches == 1;
}
static int append(RestorePlan *p, const RestoreBackend *b, uint32_t slot, uint32_t offset,
                  const void *data, uint32_t size) {
    if (p->count >= RESTORE_MAX_FIELDS || !b->allowed(b->user, slot, offset, size))
        return 0;
    for (size_t i = 0; i < p->count; i++) {
        const RestoreField *f = p->fields + i;
        if (f->object_slot == slot && offset < f->field_offset + f->size &&
            f->field_offset < offset + size)
            return 0;
    }
    RestoreField *f = p->fields + p->count;
    memset(f, 0, sizeof(*f));
    f->object_slot = slot;
    f->field_offset = offset;
    f->size = size;
    if (!b->read(b->user, slot, offset, f->expected, size))
        return 0;
    memcpy(f->desired, data, size);
    ++p->count;
    return 1;
}
int native_cache_plan(const NativeCache *c, const PartialCheckpoint *saved, const RestoreBackend *b,
                      RestorePlan *work, RestorePlan *out) {
    if (!c || !saved || !b || !b->context || !b->allowed || !b->read || !work || !out ||
        work == out || !work->count || work->count > RESTORE_MAX_FIELDS || !c->sequence ||
        c->sequence != saved->sequence || !native_cache_valid(c, saved->bodies, saved->body_count))
        return RESTORE_BAD_PLAN;
    if (!b->context(b->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    for (size_t i = 0; i < saved->body_count; i++) {
        const NativeBodyCache *v = c->bodies + i;
        if (!replace(work, (uint32_t)i, 8, &v->mass, 8) ||
            !replace(work, (uint32_t)i, 0x140, v->inertia_local, 24) ||
            !replace(work, (uint32_t)i, 0x60, v->inverse_world, 48) ||
            !replace(work, (uint32_t)i, 0xe0, v->omega, 24))
            return RESTORE_BAD_PLAN;
    }
    for (uint32_t w = 0; w < 4; w++)
        for (uint32_t s = 0; s < 8; s++) {
            if (!append(work, b, RESTORE_TIRE_BASE + w, 0x30 + s * 0x60,
                        c->sectors[w][s].first_moment, 24) ||
                !append(work, b, RESTORE_TIRE_BASE + w, 0x60 + s * 0x60, c->sectors[w][s].aggregate,
                        16))
                return RESTORE_FIELD_FORBIDDEN;
        }
    if (!b->context(b->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    *out = *work;
    return RESTORE_OK;
}
