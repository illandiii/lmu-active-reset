#include "mechanical_prepare_core.h"
#include <math.h>
#include <float.h>
static int agrees(double x, double y) {
    return isfinite(x) && isfinite(y) && x > 0 && y > 0 &&
           fabs(x - y) <= 32 * DBL_EPSILON * fabs(y);
}
int mechanical_prepare(const RigidSavedBody *saved, const RigidMassProperties *current,
                       size_t count, const MechanicalLinks *links, const MechanicalDependencies *a,
                       const MechanicalDependencies *b, RigidPreparedBody *output,
                       FuelPrepared *fuel_output) {
    if (!saved || !current || !links || !a || !b || !output || !fuel_output || count < 9 ||
        count > 32)
        return MECHANICAL_BAD_ARGUMENT;
    size_t ids[9];
    ids[0] = links->fuel;
    for (int i = 0; i < 4; i++) {
        ids[1 + i] = links->wheels[i];
        ids[5 + i] = links->tires[i];
    }
    for (int i = 0; i < 9; i++) {
        if (ids[i] >= count)
            return MECHANICAL_LINKS_INVALID;
        for (int j = 0; j < i; j++)
            if (ids[i] == ids[j])
                return MECHANICAL_LINKS_INVALID;
    }
    WheelPreparedInertia wa[4], wb[4];
    TirePreparedInertia ta[4], tb[4];
    if (wheel_prepare_inertia(a->wheels, wa) || wheel_prepare_inertia(b->wheels, wb) ||
        tire_prepare_inertia(a->tires, ta) || tire_prepare_inertia(b->tires, tb))
        return MECHANICAL_DEPENDENCY_INVALID;
    RigidMassProperties proposed[32];
    for (size_t i = 0; i < count; i++)
        proposed[i] = current[i];
    for (int w = 0; w < 4; w++) {
        if (a->wheels[w].base_130 != b->wheels[w].base_130 ||
            a->wheels[w].factor_4f0 != b->wheels[w].factor_4f0)
            return MECHANICAL_CONFIG_CHANGED;
        size_t wi = links->wheels[w], ti = links->tires[w];
        if (!agrees(saved[wi].mass.inverse_inertia_local[0], wa[w].inverse_axial) ||
            !agrees(current[wi].inverse_inertia_local[0], wb[w].inverse_axial))
            return MECHANICAL_BODY_MISMATCH;
        proposed[wi].inverse_inertia_local[0] = saved[wi].mass.inverse_inertia_local[0];
        for (int j = 0; j < 3; j++) {
            if (!agrees(saved[ti].mass.inverse_inertia_local[j], ta[w].inverse_internal[j]) ||
                !agrees(current[ti].inverse_inertia_local[j], tb[w].inverse_internal[j]))
                return MECHANICAL_BODY_MISMATCH;
            proposed[ti].inverse_inertia_local[j] = saved[ti].mass.inverse_inertia_local[j];
        }
    }
    /* fuel helper still rejects any other mass/inertia change and publishes
     * neither output until its full-body validation succeeds. */
    FuelResult r = rigid_prepare_with_fuel(saved, proposed, count, links->fuel, &a->fuel, &b->fuel,
                                           output, fuel_output);
    return r == FUEL_OK ? MECHANICAL_OK : MECHANICAL_OTHER_CHANGE;
}
