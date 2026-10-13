#include "tire_inertia_core.h"
#include <math.h>
#include <string.h>
int tire_prepare_inertia(const TireInertiaPartition saved[4], TirePreparedInertia output[4]) {
    if (!saved || !output)
        return TIRE_INERTIA_BAD_ARGUMENT;
    TirePreparedInertia prepared[4] = {0};
    for (int w = 0; w < 4; w++)
        for (int j = 0; j < 3; j++) {
            double total = saved[w].total_table[j], sum = 0;
            if (!isfinite(total) || total <= 0)
                return TIRE_INERTIA_INVALID_VALUE;
            for (int s = 0; s < 8; s++) {
                double sector = saved[w].sectors[s][j];
                if (!isfinite(sector) || sector < 0)
                    return TIRE_INERTIA_INVALID_VALUE;
                sum += sector;
            }
            double residual = total - sum, inv = 1 / residual;
            if (!isfinite(sum) || !isfinite(residual) || residual <= 0 || !isfinite(inv) ||
                inv <= 0)
                return TIRE_INERTIA_INVALID_VALUE;
            prepared[w].sector_sum[j] = sum;
            prepared[w].internal[j] = residual;
            prepared[w].inverse_internal[j] = inv;
        }
    memcpy(output, prepared, sizeof(prepared));
    return TIRE_INERTIA_OK;
}
