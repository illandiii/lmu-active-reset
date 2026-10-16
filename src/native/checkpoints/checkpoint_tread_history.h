#ifndef LMU_CHECKPOINT_TREAD_HISTORY_H
#define LMU_CHECKPOINT_TREAD_HISTORY_H
#include "checkpoint_tire_thermal.h"
/* Offset names are deliberate: exact numeric history, incomplete semantics. */
typedef struct TreadHistory {
    double fields_28[2], field_100;
} TreadHistory;
typedef struct TreadHistoryGrid {
    TreadHistory nodes[4][72][7];
} TreadHistoryGrid;
typedef struct ThermalHistoryBank {
    ThermalBank prior;
    TreadHistoryGrid history;
} ThermalHistoryBank;
int tread_history_valid(const TreadHistoryGrid *);
int thermal_history_save(ThermalHistoryBank *, uint64_t, const ThermalGrid *,
                         const TreadHistoryGrid *);
int thermal_history_prepare(const ThermalHistoryBank *, uint64_t, const ThermalGrid *,
                            const TreadHistoryGrid *, ThermalGrid *, TreadHistoryGrid *);
#endif
