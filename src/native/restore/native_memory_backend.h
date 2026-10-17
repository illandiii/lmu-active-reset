#ifndef LMU_RESTORE_NATIVE_MEMORY_25661166_H
#define LMU_RESTORE_NATIVE_MEMORY_25661166_H
#include "vehicle_memory_bindings.h"
#include "restore_flow.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Read-only, exact same binding/alias/expected-state checks as the writer.
 * A successful probe does not pin lifetime or authorize later execution. */
int restore_joint_native_preflight(const RestorePlan *, const RestoreBindings *, uintptr_t,
                                   uint32_t, int (*)(void *, uint64_t), void *);
/* Caller owns the full compatibility/lifecycle gate and physical-thread hook.
 * Bindings are copied once and retained even if context fails during rollback.
 * The observers use only its read-only preflight entry point. It does not prove
 * checkpoint coverage, retire contacts, invalidate laps or authorize a reset. */
int restore_native_memory_execute(const RestorePlan *, const RestoreBindings *,
                                  uint32_t owner_thread, int (*validate_context)(void *, uint64_t),
                                  void *context_user, RestoreReport *);
/* Combined physical/scoring plan: scoring occupies slot RESTORE_OBJECT_COUNT.
 * Validates exact scoring fields and absolute cross-object aliases, freezes
 * both bindings, and executes on the nominated thread. Fixture use only until
 * lifecycle, coverage and contact integration are validated in the adapter. */
int restore_joint_native_memory_execute(const RestorePlan *, const RestoreBindings *,
                                        uintptr_t scoring_object, uint32_t owner_thread,
                                        int (*validate_context)(void *, uint64_t),
                                        void *context_user, RestoreReport *);
int restore_joint_native_flow_execute(RestoreFlow *, RestoreFlowWork *, const RestorePlan *,
                                      const RestoreBindings *, uintptr_t, uint32_t,
                                      int (*validate_context)(void *, uint64_t), void *,
                                      const RestoreFlowOps *, RestoreFlowReport *);
#ifdef __cplusplus
}
#endif
#endif
