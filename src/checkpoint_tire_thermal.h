#ifndef LMU_CHECKPOINT_TIRE_THERMAL_H
#define LMU_CHECKPOINT_TIRE_THERMAL_H
#include <stdint.h>
typedef struct ThermalCell {
    double layers[5], coefficients[4];
} ThermalCell;
/* Canonical pool order: seven tread nodes, A10 cell, A18 cell. Row order is
 * flattened sector descriptors, never pointer-table order. No pointers saved. */
typedef struct TireThermalGrid {
    uint32_t angular_index[72];
    ThermalCell cells[72][9];
} TireThermalGrid;
typedef struct ThermalGrid {
    TireThermalGrid tires[4];
} ThermalGrid;
typedef struct ThermalBank {
    uint64_t sequence;
    ThermalGrid grid;
} ThermalBank;
int thermal_grid_valid(const ThermalGrid *);
int thermal_grid_save(ThermalBank *, uint64_t, const ThermalGrid *);
int thermal_grid_prepare(const ThermalBank *, uint64_t, const ThermalGrid *, ThermalGrid *);
#endif
