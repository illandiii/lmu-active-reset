#include "checkpoint_tread_history.h"
#include <math.h>
int tread_history_valid(const TreadHistoryGrid *g) {
    if (!g)
        return 0;
    for (int w = 0; w < 4; w++)
        for (int i = 0; i < 72; i++)
            for (int j = 0; j < 7; j++) {
                const TreadHistory *t = &g->nodes[w][i][j];
                if (!isfinite(t->fields_28[0]) || !isfinite(t->fields_28[1]) ||
                    !isfinite(t->field_100))
                    return 0;
            }
    return 1;
}
int thermal_history_save(ThermalHistoryBank *b, uint64_t seq, const ThermalGrid *g,
                         const TreadHistoryGrid *h) {
    if (!b || !tread_history_valid(h) || !thermal_grid_save(&b->prior, seq, g))
        return 0;
    b->history = *h;
    return 1;
}
int thermal_history_prepare(const ThermalHistoryBank *b, uint64_t seq, const ThermalGrid *g,
                            const TreadHistoryGrid *h, ThermalGrid *out,
                            TreadHistoryGrid *history_out) {
    if (!b || !history_out || !tread_history_valid(&b->history) || !tread_history_valid(h) ||
        !thermal_grid_prepare(&b->prior, seq, g, out))
        return 0;
    *history_out = b->history;
    return 1;
}
