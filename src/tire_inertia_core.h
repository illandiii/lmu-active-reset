#ifndef LMU_TIRE_INERTIA_CORE_H
#define LMU_TIRE_INERTIA_CORE_H
/* Owned values only. F0F4E6..F0F689 on build 14200 converts the table's
 * float inertia to double, sums eight sector contributions in order, and
 * assigns the residual to the internal body. This is NOT the tire's full
 * pressure/temperature/history state or an authorization to change inertia. */
typedef struct TireInertiaPartition {
    float total_table[3];
    double sectors[8][3];
} TireInertiaPartition;
typedef struct TirePreparedInertia {
    double sector_sum[3];
    double internal[3];
    double inverse_internal[3];
} TirePreparedInertia;
enum TireInertiaResult { TIRE_INERTIA_OK, TIRE_INERTIA_BAD_ARGUMENT, TIRE_INERTIA_INVALID_VALUE };
/* Prepare exactly four tires; publish nothing if any tire fails validation. */
int tire_prepare_inertia(const TireInertiaPartition saved[4], TirePreparedInertia output[4]);
#endif
