#ifndef LMU_CHECKPOINT_DRIVELINE_CACHE_H
#define LMU_CHECKPOINT_DRIVELINE_CACHE_H
#include "../restore/restore_bindings_25661166.h"
typedef struct DrivelineCache {
    double torque[3];
    uint32_t drive_type, reserved;
} DrivelineCache;
typedef struct DrivelineCacheBank {
    uint64_t sequence;
    DrivelineCache state;
} DrivelineCacheBank;
int driveline_cache_valid(const DrivelineCache *);
int driveline_cache_save(DrivelineCacheBank *, uint64_t, const DrivelineCache *);
int driveline_cache_prepare(const DrivelineCacheBank *, uint64_t, const DrivelineCache *,
                            DrivelineCache *);
int driveline_cache_plan(const DrivelineCacheBank *, uint64_t, const RestoreBackend *,
                         RestorePlan *, RestorePlan *);
#endif
