#ifndef LMU_CHECKPOINT_TRANSMISSION_PARTIAL_H
#define LMU_CHECKPOINT_TRANSMISSION_PARTIAL_H
#include "checkpoint_vehicle_partial.h"
typedef struct TransmissionPartialState {
    double shaft_0, shaft_8, input_18, input_20, input_30, input_38;
    int32_t gear_64;
    uint32_t reserved;
} TransmissionPartialState;
typedef struct TransmissionBank {
    VehiclePartialBank vehicle;
    TransmissionPartialState transmission;
} TransmissionBank;
typedef struct TransmissionPreparation {
    VehiclePartialPreparation vehicle;
    TransmissionPartialState transmission;
} TransmissionPreparation;
int checkpoint_save_transmission(TransmissionBank *, const PartialCheckpoint *,
                                 const MechanicalLinks *, const MechanicalDependencies *,
                                 const TirePersistentState[4], const PowertrainPartialState *,
                                 const TransmissionPartialState *);
int checkpoint_prepare_transmission(const TransmissionBank *, const CheckpointIdentity *,
                                    const RigidMassProperties *, size_t, const ConstraintRecipe *,
                                    size_t, const MechanicalDependencies *,
                                    const TirePersistentState[4], const PowertrainPartialState *,
                                    const TransmissionPartialState *, TransmissionPreparation *);
#endif
