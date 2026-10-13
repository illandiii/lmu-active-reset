#ifndef LMU_CHECKPOINT_CACHED_MASS_H
#define LMU_CHECKPOINT_CACHED_MASS_H
#include "checkpoint_transmission_partial.h"
typedef struct CachedMassBank {
    TransmissionBank state;
    double tire_mass_cache[4];
} CachedMassBank;
typedef struct CachedMassPreparation {
    TransmissionPreparation state;
    double tire_mass_cache[4];
} CachedMassPreparation;
/* Native body mass caches are distinct state: sector updates may follow the
 * last mass update. Never infer these caches from current sector values. */
int checkpoint_save_cached_mass(CachedMassBank *, const PartialCheckpoint *,
                                const MechanicalLinks *, const MechanicalDependencies *,
                                const TirePersistentState[4], const PowertrainPartialState *,
                                const TransmissionPartialState *, const double[4]);
int checkpoint_prepare_cached_mass(const CachedMassBank *, const CheckpointIdentity *,
                                   const RigidMassProperties *, size_t, const ConstraintRecipe *,
                                   size_t, const MechanicalDependencies *,
                                   const TirePersistentState[4], const PowertrainPartialState *,
                                   const TransmissionPartialState *, const double[4],
                                   CachedMassPreparation *);
#endif
