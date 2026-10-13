#ifndef LMU_CHECKPOINT_SOLVER_HISTORY_H
#define LMU_CHECKPOINT_SOLVER_HISTORY_H
#include "checkpoint_cached_mass.h"
typedef struct BodyHistory {
    double force[3], torque[3];
    uint32_t renormalization, reserved;
} BodyHistory;
typedef struct SolverRowHistory {
    double bias, coefficient, jacobian[12], weighted[12];
} SolverRowHistory;
typedef struct SolverHistory {
    BodyHistory bodies[32];
    SolverRowHistory rows[64][3];
    double driveline_timer_46c8;
} SolverHistory;
typedef struct HistoryBank {
    CachedMassBank state;
    SolverHistory history;
} HistoryBank;
typedef struct HistoryPreparation {
    CachedMassPreparation state;
    SolverHistory history;
} HistoryPreparation;
int checkpoint_save_history(HistoryBank *, const PartialCheckpoint *, const MechanicalLinks *,
                            const MechanicalDependencies *, const TirePersistentState[4],
                            const PowertrainPartialState *, const TransmissionPartialState *,
                            const double[4], const SolverHistory *);
int checkpoint_prepare_history(const HistoryBank *, const CheckpointIdentity *,
                               const RigidMassProperties *, size_t, const ConstraintRecipe *,
                               size_t, const MechanicalDependencies *, const TirePersistentState[4],
                               const PowertrainPartialState *, const TransmissionPartialState *,
                               const double[4], const SolverHistory *, HistoryPreparation *);
#endif
