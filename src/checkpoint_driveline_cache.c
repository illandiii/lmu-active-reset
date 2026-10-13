#include "checkpoint_driveline_cache.h"
#include <math.h>
#include <string.h>
int driveline_cache_valid(const DrivelineCache *s) {
    return s && !s->reserved && s->drive_type <= 3 && isfinite(s->torque[0]) &&
           isfinite(s->torque[1]) && isfinite(s->torque[2]);
}
int driveline_cache_save(DrivelineCacheBank *b, uint64_t sequence, const DrivelineCache *s) {
    if (!b || !sequence || !driveline_cache_valid(s))
        return 0;
    b->state = *s;
    b->sequence = sequence;
    return 1;
}
int driveline_cache_prepare(const DrivelineCacheBank *b, uint64_t sequence,
                            const DrivelineCache *current, DrivelineCache *out) {
    if (!b || !out || !sequence || b->sequence != sequence || !driveline_cache_valid(&b->state) ||
        !driveline_cache_valid(current) || b->state.drive_type != current->drive_type)
        return 0;
    *out = b->state;
    return 1;
}
int driveline_cache_plan(const DrivelineCacheBank *b, uint64_t sequence,
                         const RestoreBackend *backend, RestorePlan *work, RestorePlan *out) {
    if (!b || !sequence || b->sequence != sequence || !driveline_cache_valid(&b->state) ||
        !backend || !backend->context || !backend->allowed || !backend->read || !work || !out ||
        work == out || !work->count || work->count > RESTORE_MAX_FIELDS - 3)
        return RESTORE_BAD_PLAN;
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    const uint32_t offsets[3] = {0x49d8, 0x4a18, 0x4a58};
    for (unsigned i = 0; i < 3; i++) {
        for (size_t j = 0; j < work->count; j++) {
            const RestoreField *f = work->fields + j;
            if (f->object_slot == 104 && offsets[i] < f->field_offset + f->size &&
                f->field_offset < offsets[i] + 8)
                return RESTORE_BAD_PLAN;
        }
        if (!backend->allowed(backend->user, 104, offsets[i], 8))
            return RESTORE_FIELD_FORBIDDEN;
        RestoreField *f = work->fields + work->count;
        memset(f, 0, sizeof(*f));
        f->object_slot = 104;
        f->field_offset = offsets[i];
        f->size = 8;
        if (!backend->read(backend->user, 104, offsets[i], f->expected, 8))
            return RESTORE_READ_FAILED;
        memcpy(f->desired, b->state.torque + i, 8);
        ++work->count;
    }
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    *out = *work;
    return 0;
}
