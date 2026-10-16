#ifndef LMU_CHECKPOINT_NATIVE_CACHE_H
#define LMU_CHECKPOINT_NATIVE_CACHE_H
#include "../restore/restore_plan_25661166.h"
/* Numeric fields only. These caches are captured at the same Dynamics boundary
 * as the base checkpoint; they must not be synthesized from reciprocal values. */
typedef struct NativeBodyCache {
    double mass, inertia_local[3], inverse_world[6], omega[3];
} NativeBodyCache;
typedef struct NativeSectorCache {
    double first_moment[3], aggregate[2];
} NativeSectorCache;
typedef struct NativeCache {
    uint64_t sequence;
    NativeBodyCache bodies[32];
    NativeSectorCache sectors[4][8];
} NativeCache;
int native_cache_valid(const NativeCache *, const RigidSavedBody *, size_t);
/* Extends a previously built partial plan in workspace. Never changes output on
 * failure. Exact cached values replace derived ones; unmapped tire nodes remain
 * excluded, so this is still NOT a complete vehicle restore. */
int native_cache_plan(const NativeCache *, const PartialCheckpoint *, const RestoreBackend *,
                      RestorePlan *workspace, RestorePlan *output);
#endif
