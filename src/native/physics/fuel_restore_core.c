#include "fuel_restore_core.h"
#include <math.h>
#include <float.h>
static int mass_of(const FuelSnapshot *s, double *mass) {
    if (!isfinite(s->quantity) || s->quantity < 0 || !isfinite(s->quantity_rate) ||
        !isfinite(s->density) || s->density <= 0 || !isfinite(s->base_mass) || s->base_mass < 0 ||
        !isfinite(s->additional_mass) || s->additional_mass < 0)
        return 0;
    *mass = s->quantity * s->density + s->base_mass + s->additional_mass;
    return isfinite(*mass) && *mass > 0 && isfinite(1 / *mass) && 1 / *mass > 0;
}
static int same_inverse(double observed, double mass) {
    double inverse = 1 / mass;
    return isfinite(observed) && observed > 0 &&
           fabs(observed - inverse) <= 32 * DBL_EPSILON * fabs(inverse);
}
FuelResult fuel_prepare_restore(const FuelSnapshot *saved, const FuelSnapshot *current,
                                FuelPrepared *output) {
    if (!saved || !current || !output)
        return FUEL_BAD_ARGUMENT;
    double a, b;
    if (!mass_of(saved, &a) || !mass_of(current, &b))
        return FUEL_INVALID_VALUE;
    if (saved->density != current->density || saved->base_mass != current->base_mass ||
        saved->additional_mass != current->additional_mass)
        return FUEL_CONFIGURATION_CHANGED;
    FuelPrepared prepared = {*saved, a, 1 / a};
    *output = prepared;
    return FUEL_OK;
}
FuelResult rigid_prepare_with_fuel(const RigidSavedBody *bodies,
                                   const RigidMassProperties *current_mass, size_t count,
                                   size_t fuel_body, const FuelSnapshot *saved_fuel,
                                   const FuelSnapshot *current_fuel, RigidPreparedBody *output,
                                   FuelPrepared *fuel_output) {
    if (!bodies || !current_mass || !output || !fuel_output || !saved_fuel || !current_fuel ||
        !count || count > 32 || fuel_body >= count)
        return FUEL_BAD_ARGUMENT;
    FuelPrepared fuel;
    FuelResult status = fuel_prepare_restore(saved_fuel, current_fuel, &fuel);
    if (status != FUEL_OK)
        return status;
    double current_body_mass;
    if (!mass_of(current_fuel, &current_body_mass))
        return FUEL_INVALID_VALUE;
    if (!same_inverse(bodies[fuel_body].mass.inverse_mass, fuel.body_mass) ||
        !same_inverse(current_mass[fuel_body].inverse_mass, current_body_mass))
        return FUEL_BODY_MISMATCH;
    RigidMassProperties proposed[32];
    RigidPreparedBody prepared[32];
    for (size_t i = 0; i < count; i++)
        proposed[i] = current_mass[i];
    // Preserve the exact recorded representation after checking its formula.
    proposed[fuel_body].inverse_mass = bodies[fuel_body].mass.inverse_mass;
    if (rigid_prepare_restore(bodies, proposed, count, prepared) != RIGID_PREPARE_OK)
        return FUEL_OTHER_BODY_CHANGED;
    for (size_t i = 0; i < count; i++)
        output[i] = prepared[i];
    *fuel_output = fuel;
    return FUEL_OK;
}
