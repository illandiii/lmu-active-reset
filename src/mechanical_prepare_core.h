#ifndef LMU_MECHANICAL_PREPARE_CORE_H
#define LMU_MECHANICAL_PREPARE_CORE_H
#include "fuel_restore_core.h"
#include "wheel_inertia_core.h"
#include "tire_inertia_core.h"
typedef struct MechanicalDependencies {
    FuelSnapshot fuel;
    WheelInertiaInputs wheels[4];
    TireInertiaPartition tires[4];
} MechanicalDependencies;
typedef struct MechanicalLinks {
    size_t fuel, wheels[4], tires[4];
} MechanicalLinks;
enum MechanicalResult {
    MECHANICAL_OK,
    MECHANICAL_BAD_ARGUMENT,
    MECHANICAL_LINKS_INVALID,
    MECHANICAL_DEPENDENCY_INVALID,
    MECHANICAL_CONFIG_CHANGED,
    MECHANICAL_BODY_MISMATCH,
    MECHANICAL_OTHER_CHANGE
};
/* Prepare owned numerical bodies from explicit saved/current dependencies.
 * No process pointers/writes. This only resolves these mass/inertia relations;
 * upstream tire/brake history, constraints, drivetrain and identity/sync remain
 * prerequisites of any real checkpoint adapter. Nothing is published on failure. */
int mechanical_prepare(const RigidSavedBody *saved, const RigidMassProperties *current,
                       size_t count, const MechanicalLinks *links,
                       const MechanicalDependencies *saved_deps,
                       const MechanicalDependencies *current_deps, RigidPreparedBody *output,
                       FuelPrepared *fuel_output);
#endif
