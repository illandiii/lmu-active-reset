#ifndef LMU_RIGID_RESTORE_CORE_H
#define LMU_RIGID_RESTORE_CORE_H
#include <stddef.h>

#define RIGID_RESTORE_MAX_BODIES 32

/* Owned numerical values only. No process pointers, object bytes or contacts.
 * This is the kinematic preparation component, NOT a live LMU restore adapter.
 */
typedef struct RigidMassProperties {
    double inverse_mass;
    double inverse_inertia_local[3];
} RigidMassProperties;

typedef struct RigidSavedBody {
    RigidMassProperties mass;
    double position_world[3];
    double velocity_world[3];
    double orientation[9]; /* row-major local -> world */
    double momentum_world[3];
} RigidSavedBody;

typedef struct RigidPreparedBody {
    RigidSavedBody state;
    double inverse_inertia_world[6]; /* xx, xy, yy, xz, yz, zz */
    double angular_velocity_world[3];
} RigidPreparedBody;

typedef enum RigidPrepareResult {
    RIGID_PREPARE_OK = 0,
    RIGID_PREPARE_BAD_ARGUMENT,
    RIGID_PREPARE_INVALID_VALUE,
    RIGID_PREPARE_INVALID_ROTATION,
    RIGID_PREPARE_MASS_CHANGED
} RigidPrepareResult;

/* Validate every body and rebuild I^-1_world and omega before publishing any
 * output. On failure the caller-owned output stays untouched, including when
 * the LAST body is invalid. Mass changes (fuel, damage, setup) are refused.
 * An adapter must separately establish identity/lifetime, synchronization,
 * complete subsystem coverage and contact/force reconstruction before writing
 * any of these values to a game. This core supplies none of those guarantees.
 */
RigidPrepareResult rigid_prepare_restore(const RigidSavedBody *saved,
                                         const RigidMassProperties *current_mass, size_t count,
                                         RigidPreparedBody *output);

/* Explicitly PARTIAL kinematic preparation: preserve saved pose, linear and
 * angular velocity while retaining CURRENT mass/inertia (fuel/thermal state).
 * Recompute L for current inertia. This does not restore saved energy, fuel,
 * pressure or tire history. The strict full-state preparation above is unchanged.
 * All validation precedes output publication; this still supplies no live write
 * authorization, object lifetime guarantee or cache-reconstruction adapter.
 */
RigidPrepareResult rigid_prepare_kinematics_current_mass(const RigidSavedBody *saved,
                                                         const RigidMassProperties *current_mass,
                                                         size_t count, RigidPreparedBody *output);
#endif
