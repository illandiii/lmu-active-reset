#ifndef LMU_CHECKPOINT_ENGINE_INTEGRATION_H
#define LMU_CHECKPOINT_ENGINE_INTEGRATION_H
#include "checkpoint_solver_history.h"
typedef struct EngineIntegration {
    double outputs_208[7], channel_state[2][2];
    float fields_240[3];
    uint32_t channel_count;
} EngineIntegration;
typedef struct EngineBank {
    HistoryBank prior;
    EngineIntegration integration;
} EngineBank;
typedef struct EnginePreparation {
    HistoryPreparation prior;
    EngineIntegration integration;
} EnginePreparation;
int checkpoint_save_engine(EngineBank *, const PartialCheckpoint *, const MechanicalLinks *,
                           const MechanicalDependencies *, const TirePersistentState[4],
                           const PowertrainPartialState *, const TransmissionPartialState *,
                           const double[4], const SolverHistory *, const EngineIntegration *);
int checkpoint_prepare_engine(const EngineBank *, const CheckpointIdentity *,
                              const RigidMassProperties *, size_t, const ConstraintRecipe *, size_t,
                              const MechanicalDependencies *, const TirePersistentState[4],
                              const PowertrainPartialState *, const TransmissionPartialState *,
                              const double[4], const SolverHistory *, const EngineIntegration *,
                              EnginePreparation *);
#endif
