/* Local fixture bridge only, never loaded in LMU. */
#include "rigid_restore_core.h"
__declspec(dllexport) int fixture_prepare(const RigidSavedBody *saved,
                                          const RigidMassProperties *current, size_t count,
                                          RigidPreparedBody *out, int partial) {
    return partial ? rigid_prepare_kinematics_current_mass(saved, current, count, out)
                   : rigid_prepare_restore(saved, current, count, out);
}
