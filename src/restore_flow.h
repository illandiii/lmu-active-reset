#ifndef LMU_RESTORE_FLOW_H
#define LMU_RESTORE_FLOW_H
#include "restore_transaction.h"
/* Synchronous owner-thread boundary: enter pins bindings and prevents world or
 * scoring advancement through leave. Contacts must rebuild AND verify all four
 * wheels without integration. Recovery uses the same pinned objects. */
typedef struct RestoreFlowOps {
    void *user;
    int (*enter)(void *, uint64_t);
    int (*owner_alive)(void *, uint64_t);
    int (*contacts)(void *, uint64_t, int restoring_original);
    void (*leave)(void *, int resume);
    void (*quarantine)(void *, int cause);
} RestoreFlowOps;
typedef struct RestoreFlow {
    int busy, quarantined;
    uint64_t completed, compensated;
} RestoreFlow;
typedef struct RestoreFlowWork {
    RestorePlan frozen;
} RestoreFlowWork;
typedef struct RestoreFlowReport {
    int result, cause, contact_calls;
    RestoreReport forward, recovery;
} RestoreFlowReport;
enum { FLOW_OK, FLOW_COMPENSATED, FLOW_REJECTED, FLOW_QUARANTINED, FLOW_BUSY, FLOW_BAD_ARGUMENT };
enum {
    FLOW_CONTACT_FAILED = 100,
    FLOW_CONTEXT_EXPIRED = 101,
    FLOW_OWNER_LOST = 102,
    FLOW_NATIVE_LOCK_LOST = 103
};
int restore_flow_execute(RestoreFlow *, RestoreFlowWork *, const RestorePlan *,
                         const RestoreBackend *, const RestoreFlowOps *, RestoreFlowReport *);
#endif
