#ifndef LMU_WHEEL_INERTIA_CORE_H
#define LMU_WHEEL_INERTIA_CORE_H
/* Numerical dependency observed at 14200 EE1984..EE1A1C and ECAE58..ECAEE2.
 * Field names intentionally retain offsets until upstream semantics are proven.
 * This is not a full brake/thermal subsystem snapshot or live writer. */
typedef struct WheelInertiaInputs {
    double base_130, term_4b8, term_5d8, factor_4f0;
} WheelInertiaInputs;
typedef struct WheelPreparedInertia {
    double axial, inverse_axial;
} WheelPreparedInertia;
enum WheelInertiaResult {
    WHEEL_INERTIA_OK,
    WHEEL_INERTIA_BAD_ARGUMENT,
    WHEEL_INERTIA_INVALID_VALUE
};
int wheel_prepare_inertia(const WheelInertiaInputs saved[4], WheelPreparedInertia output[4]);
#endif
