#include "scoring_plan_25661166.h"
#include <string.h>
int scoring_field_allowed_25661166(uint32_t slot, uint32_t offset, uint32_t size) {
    return slot == 0 && size == 4 && (offset == 0x160 || offset == 0x27290 || offset == 0x27360);
}
int scoring_plan_25661166(const LapGuardFlags *before, uint64_t epoch, RestorePlan *out) {
    LapGuardFlags after;
    if (!out || !epoch || !lap_guard_prepare(before, &after))
        return 0;
    const uint32_t offsets[3] = {0x160, 0x27290, 0x27360};
    const int32_t values[3] = {before->current, before->remembered, before->last_computed};
    RestoreField fields[3];
    memset(fields, 0, sizeof(fields));
    for (unsigned i = 0; i < 3; i++) {
        fields[i].field_offset = offsets[i];
        fields[i].size = 4;
        memcpy(fields[i].expected, &values[i], 4);
        /* desired zero is the entire three-cache invalidation. */
    }
    memcpy(out->fields, fields, sizeof(fields));
    out->epoch = epoch;
    out->count = 3;
    return 1;
}
