#ifndef LMU_FUEL_RESTORE_CORE_H
#define LMU_FUEL_RESTORE_CORE_H
#include "rigid_restore_core.h"
typedef struct FuelSnapshot {
    double quantity, quantity_rate;
    double density, base_mass, additional_mass;
} FuelSnapshot;
typedef struct FuelPrepared {
    FuelSnapshot saved;
    double body_mass, body_inverse_mass;
} FuelPrepared;
typedef enum FuelResult {
    FUEL_OK = 0,
    FUEL_BAD_ARGUMENT,
    FUEL_INVALID_VALUE,
    FUEL_CONFIGURATION_CHANGED,
    FUEL_BODY_MISMATCH,
    FUEL_OTHER_BODY_CHANGED
} FuelResult;
/* Native C6E170 arithmetic, owned values only. No fuel/physics writes.
 * Quantity limits, episode identity and complete energy/thermal state must be
 * validated by a live adapter; this component does not provide that adapter.
 */
FuelResult fuel_prepare_restore(const FuelSnapshot *saved, const FuelSnapshot *current,
                                FuelPrepared *output);
/* Allow only a fuel-explained inverse-mass change on one identified body.
 * All inertia changes and unrelated mass changes still fail. Publish neither
 * output until the entire rigid group and both fuel/body relations validate.
 */
FuelResult rigid_prepare_with_fuel(const RigidSavedBody *bodies,
                                   const RigidMassProperties *current_mass, size_t count,
                                   size_t fuel_body, const FuelSnapshot *saved_fuel,
                                   const FuelSnapshot *current_fuel, RigidPreparedBody *output,
                                   FuelPrepared *fuel_output);
#endif
