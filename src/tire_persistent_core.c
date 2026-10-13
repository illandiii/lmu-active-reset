#include "tire_persistent_core.h"
#include <math.h>
#include <float.h>
#include <string.h>
static int same(double a, double b) {
    return isfinite(a) && isfinite(b) && a > 0 && b > 0 &&
           fabs(a - b) <= 32 * DBL_EPSILON * fabs(b);
}
int tire_persistent_validate(const TirePersistentState s[4], const MechanicalDependencies *d,
                             const RigidMassProperties *m, size_t n, const MechanicalLinks *l,
                             MechanicalDependencies *out) {
    if (!s || !d || !m || !l || !out || n < 9 || n > 32)
        return 1;
    MechanicalDependencies result = *d;
    for (int w = 0; w < 4; w++) {
        if (l->tires[w] >= n)
            return 1;
        for (int j = 0; j < w; j++)
            if (l->tires[j] == l->tires[w])
                return 1;
        const TirePersistentState *t = s + w;
        /* Native F1257B can store 0x80000 before the next step masks to
         * 0x7ffff. Preserve the full counter; it is not a bounded index. */
        if (!isfinite(t->total_mass) || t->total_mass <= 0 || t->reserved ||
            !isfinite(t->thermal_fb0) || !isfinite(t->thermal_fb8) || !isfinite(t->accumulated_fc0))
            return 2;
        double mass = t->total_mass;
        for (int sector = 0; sector < 8; sector++) {
            if (!isfinite(t->sector_mass[sector]) || t->sector_mass[sector] < 0)
                return 2;
            mass -= t->sector_mass[sector];
            for (int j = 0; j < 3; j++)
                if (!isfinite(d->tires[w].sectors[sector][j]) || d->tires[w].sectors[sector][j] < 0)
                    return 2;
        }
        if (!same(m[l->tires[w]].inverse_mass, 1 / mass))
            return 3;
        memset(result.tires[w].sectors, 0, sizeof(result.tires[w].sectors));
        for (int j = 0; j < 3; j++) {
            double cache = t->cached_sector_inertia[j], total = d->tires[w].total_table[j];
            if (!isfinite(cache) || cache < 0 || !isfinite(total) || total <= cache)
                return 2;
            if (!same(m[l->tires[w]].inverse_inertia_local[j], 1 / (total - cache)))
                return 3;
            result.tires[w].sectors[0][j] = cache;
        }
    }
    *out = result;
    return 0;
}
