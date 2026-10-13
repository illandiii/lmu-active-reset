#include "tire_inertia_core.h"
__declspec(dllexport) int fixture_tire_inertia(const TireInertiaPartition *saved,
                                               TirePreparedInertia *output) {
    return tire_prepare_inertia(saved, output);
}
