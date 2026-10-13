#include "fuel_restore_core.h"
/* Local replay only, never loaded into LMU. */
__declspec(dllexport) int fixture_prepare_fuel(const RigidSavedBody *saved,
                                               const RigidMassProperties *current, size_t count,
                                               size_t index, const FuelSnapshot *before,
                                               const FuelSnapshot *after, RigidPreparedBody *output,
                                               FuelPrepared *fuel) {
    return rigid_prepare_with_fuel(saved, current, count, index, before, after, output, fuel);
}
