#include "checkpoint_tire_thermal.h"
#include <math.h>
#include <string.h>
int thermal_grid_valid(const ThermalGrid *g) {
    if (!g)
        return 0;
    for (int w = 0; w < 4; w++)
        for (int i = 0; i < 72; i++) {
            const TireThermalGrid *t = g->tires + w;
            if (t->angular_index[i] >= 256)
                return 0;
            for (int j = 0; j < i; j++)
                if (t->angular_index[i] == t->angular_index[j])
                    return 0;
            for (int j = 0; j < 9; j++) {
                for (int k = 0; k < 5; k++)
                    if (!isfinite(t->cells[i][j].layers[k]))
                        return 0;
                for (int k = 0; k < 4; k++)
                    if (!isfinite(t->cells[i][j].coefficients[k]) ||
                        t->cells[i][j].coefficients[k] < 0)
                        return 0;
            }
        }
    return 1;
}
int thermal_grid_save(ThermalBank *b, uint64_t sequence, const ThermalGrid *g) {
    if (!b || !sequence || !thermal_grid_valid(g))
        return 0;
    b->grid = *g;
    b->sequence = sequence;
    return 1;
}
int thermal_grid_prepare(const ThermalBank *b, uint64_t sequence, const ThermalGrid *current,
                         ThermalGrid *out) {
    if (!b || !out || !sequence || sequence != b->sequence || !thermal_grid_valid(&b->grid) ||
        !thermal_grid_valid(current))
        return 0;
    for (int w = 0; w < 4; w++)
        if (memcmp(b->grid.tires[w].angular_index, current->tires[w].angular_index,
                   sizeof(current->tires[w].angular_index)))
            return 0;
    *out = b->grid;
    return 1;
}
