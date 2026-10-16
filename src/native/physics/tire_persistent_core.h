#ifndef LMU_TIRE_PERSISTENT_CORE_H
#define LMU_TIRE_PERSISTENT_CORE_H
#include <stdint.h>
#include "mechanical_prepare_core.h"
typedef struct TirePersistentState {
    double total_mass, sector_mass[8], cached_sector_inertia[3];
    uint32_t update_phase, reserved;
    /* Native fields; complete thermodynamic meaning is not yet established. */
    double thermal_fb0, thermal_fb8, accumulated_fc0;
} TirePersistentState;
/* Validate the captured body against persistent native cache, not an assumed
 * freshly recomputed sector sum. Preserve the original sector state separately.
 * Supplies an owned equivalent inertia partition to the existing math core. */
int tire_persistent_validate(const TirePersistentState states[4],
                             const MechanicalDependencies *dependencies,
                             const RigidMassProperties *mass, size_t count,
                             const MechanicalLinks *links, MechanicalDependencies *normalized);
#endif
