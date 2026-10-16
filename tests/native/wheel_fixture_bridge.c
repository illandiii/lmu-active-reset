#include "../../src/native/physics/wheel_restore_core.h"
/* Export for local replay fixtures only. Never loaded into LMU. */
__declspec(dllexport) int fixture_wheel_prepare(const WheelSavedSpin *spin,
                                                const RigidSavedBody *hubs,
                                                const RigidMassProperties *mass,
                                                WheelPreparedSpin *out) {
    return (int)wheel_prepare_spin(spin, hubs, mass, out);
}
__declspec(dllexport) int fixture_wheel_validate(const WheelSavedSpin *spin,
                                                 const RigidSavedBody *hubs,
                                                 const RigidSavedBody *wheels,
                                                 WheelPreparedSpin *out) {
    return (int)wheel_prepare_consistent_checkpoint(spin, hubs, wheels, out);
}
