#ifndef LMU_CHECKPOINT_DRIVELINE_OUTPUTS_H
#define LMU_CHECKPOINT_DRIVELINE_OUTPUTS_H
#include "../restore/restore_bindings_25661166.h"
/* ECD670 copies ADA710 outputs [1..4] into these HDV fields. REA evidence:
 * analysis/rea-25661166/live-code/ecd670.json. Names intentionally remain offsets.
 * This does not cover gearbox/clutch internals or terminal damage state. */
typedef struct DrivelineOutputs {
    double hdv_46d0, hdv_46e0, hdv_46e8, hdv_4b38;
} DrivelineOutputs;
typedef struct DrivelineOutputsBank {
    uint64_t sequence;
    DrivelineOutputs state;
} DrivelineOutputsBank;
int driveline_outputs_valid(const DrivelineOutputs *);
int driveline_outputs_save(DrivelineOutputsBank *, uint64_t, const DrivelineOutputs *);
int driveline_outputs_prepare(const DrivelineOutputsBank *, uint64_t, const DrivelineOutputs *,
                              DrivelineOutputs *);
int driveline_outputs_plan(const DrivelineOutputsBank *, uint64_t, const RestoreBackend *,
                           RestorePlan *, RestorePlan *);
#endif
