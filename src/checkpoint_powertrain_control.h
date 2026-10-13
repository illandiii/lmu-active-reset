#ifndef LMU_CHECKPOINT_POWERTRAIN_CONTROL_H
#define LMU_CHECKPOINT_POWERTRAIN_CONTROL_H
#include "restore_bindings_25661166.h"
/* Offset-named state from REA C6FA00/C6FBB0 at HDV+4740. This is partial
 * powertrain control, not proof of complete gearbox/clutch coverage. */
typedef struct PowertrainControl {
    double ramp_d0, timer_e8, factor_f0, factor_f8, engine_50, engine_1b0;
    uint32_t index_d8, index_dc;
    uint8_t active_e0, latched_e1, reserved[6];
    uint32_t table_counts[4];
    uint64_t table_token;
} PowertrainControl;
typedef struct PowertrainControlBank {
    uint64_t sequence;
    PowertrainControl state;
} PowertrainControlBank;
int powertrain_control_valid(const PowertrainControl *);
int powertrain_control_save(PowertrainControlBank *, uint64_t, const PowertrainControl *);
int powertrain_control_prepare(const PowertrainControlBank *, uint64_t, const PowertrainControl *,
                               PowertrainControl *);
int powertrain_control_plan(const PowertrainControlBank *, uint64_t, const RestoreBackend *,
                            RestorePlan *, RestorePlan *);
#endif
