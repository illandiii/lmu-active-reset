#include "checkpoint_driveline_outputs.h"
#include <math.h>
#include <string.h>
int driveline_outputs_valid(const DrivelineOutputs *s) {
    return s && isfinite(s->hdv_46d0) && isfinite(s->hdv_46e0) && isfinite(s->hdv_46e8) &&
           isfinite(s->hdv_4b38);
}
int driveline_outputs_save(DrivelineOutputsBank *b, uint64_t sequence, const DrivelineOutputs *s) {
    if (!b || !sequence || !driveline_outputs_valid(s))
        return 0;
    b->state = *s;
    b->sequence = sequence;
    return 1;
}
int driveline_outputs_prepare(const DrivelineOutputsBank *b, uint64_t sequence,
                              const DrivelineOutputs *current, DrivelineOutputs *out) {
    if (!b || !out || !sequence || b->sequence != sequence || !driveline_outputs_valid(&b->state) ||
        !driveline_outputs_valid(current))
        return 0;
    *out = b->state;
    return 1;
}
int driveline_outputs_plan(const DrivelineOutputsBank *b, uint64_t sequence,
                           const RestoreBackend *backend, RestorePlan *work, RestorePlan *out) {
    if (!b || !sequence || b->sequence != sequence || !driveline_outputs_valid(&b->state) ||
        !backend || !backend->context || !backend->allowed || !backend->read || !work || !out ||
        work == out || !work->count || work->count > RESTORE_MAX_FIELDS - 4)
        return RESTORE_BAD_PLAN;
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    const uint32_t offsets[4] = {0x46d0, 0x46e0, 0x46e8, 0x4b38};
    const double values[4] = {b->state.hdv_46d0, b->state.hdv_46e0, b->state.hdv_46e8,
                              b->state.hdv_4b38};
    for (unsigned i = 0; i < 4; i++) {
        for (size_t j = 0; j < work->count; j++) {
            const RestoreField *f = work->fields + j;
            if (f->object_slot == 104 &&
                (uint64_t)offsets[i] < (uint64_t)f->field_offset + f->size &&
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
        memcpy(f->desired, values + i, 8);
        ++work->count;
    }
    if (!backend->context(backend->user, work->epoch))
        return RESTORE_CONTEXT_CHANGED;
    *out = *work;
    return RESTORE_OK;
}
