#ifndef LMU_WHEEL_RESTORE_CORE_H
#define LMU_WHEEL_RESTORE_CORE_H
#include "rigid_restore_core.h"

/* Owned values for the attached, undeformed branch only. No native pointers.
 * Parent identity and suspension constraints must be resolved by the adapter.
 */
typedef struct WheelSavedSpin {
    double angle;
    double angular_rate;
    unsigned int attached;
    unsigned int deformed;
} WheelSavedSpin;

typedef struct WheelPreparedSpin {
    WheelSavedSpin spin;
    double orientation[9];
    double angular_velocity_world[3];
    double angular_momentum_world[3];
    double inverse_inertia_world[6];
} WheelPreparedSpin;

/* Prepare all FOUR wheel bindings, publishing nothing on any failure.
 * Rebuild rotation, omega, L and world inertia from saved parent and spin.
 * Local math only: does not write LMU, rebuild contacts, or restore tire,
 * suspension, drivetrain, fuel, damage, thermal or hybrid histories.
 */
RigidPrepareResult wheel_prepare_spin(const WheelSavedSpin saved[4], const RigidSavedBody hubs[4],
                                      const RigidMassProperties wheel_mass[4],
                                      WheelPreparedSpin output[4]);

/* Reject a saved snapshot whose wheel pose/omega disagrees with its hub/spin.
 * This catches phase mismatch, but is not a replacement for thread ownership.
 * Output is published only after all four saved bodies agree within 1e-8.
 */
RigidPrepareResult wheel_prepare_consistent_checkpoint(const WheelSavedSpin saved[4],
                                                       const RigidSavedBody hubs[4],
                                                       const RigidSavedBody wheels[4],
                                                       WheelPreparedSpin output[4]);
#endif
