#ifndef LMU_LAP_GUARD_CORE_H
#define LMU_LAP_GUARD_CORE_H
#include <stdint.h>

/* Value payload only; no process pointers and no record/lap-time fields.
 * Offsets are build-specific (see compatibility/14200-scoring-research.json),
 * apply to native scoring, NOT telemetry, and do not authorize game writes.
 * The owner-thread adapter must acquire a coherent view and validate context.
 */
typedef struct LapGuardFlags {
    int32_t current;       /* +0x160 */
    int32_t remembered;    /* 14130: +27288; 14200: +27290 */
    int32_t last_computed; /* 14130: +27354; older 14200: +2735C; Steam 25661166: +27360 */
    uint8_t next_timed;    /* +0x164; existing next-lap restriction is preserved */
    uint8_t off_track;     /* 14130: +272B0; 14200: +272B8 */
} LapGuardFlags;

static inline int lap_guard_flags_valid(const LapGuardFlags *s) {
    return s && s->current >= 0 && s->current <= 2 && s->remembered >= 0 && s->remembered <= 2 &&
           s->last_computed >= 0 && s->last_computed <= 2 && s->next_timed <= 1 &&
           s->off_track <= 1;
}

/* Prepare the entire three-field update before an adapter may write anything.
 * Zero excludes the interrupted attempt from both lap counting and lap timing.
 * Zero the cached outputs too: otherwise a following valid lap can inherit a
 * stale cached zero when its current=2 happens to equal last_computed=2.
 * This function only writes a caller-owned value object, never live game RAM.
 */
static inline int lap_guard_prepare(const LapGuardFlags *before, LapGuardFlags *after) {
    if (!after || !lap_guard_flags_valid(before))
        return 0;
    *after = *before;
    after->current = 0;
    after->remembered = 0;
    after->last_computed = 0;
    return 1;
}
#endif
