#ifndef LMU_CONTACT_BATCH_H
#define LMU_CONTACT_BATCH_H
#include <windows.h>
#include <stdint.h>
/* Build-specific, grounded-contact adapter. Caller pins object lifetimes and
   prevents integration for the entire operation. Failure after write_attempts
   requires enclosing restore-flow compensation or quarantine. Not a live gate. */
struct ContactBatch {
    uintptr_t tire[4];
    double dt[4], horizon[4], angular[4];
    void *world;
    DWORD owner_thread;
    uint64_t epoch;
    void *user;
    int (*owner_alive)(void *, uint64_t);
    void(WINAPI *refresh)(void *, double, double, void *, double);
};
struct ContactBatchReport {
    unsigned preflight_complete, completed_mask, write_attempts, refresh_calls;
};
/* Read-only probe; does not preserve a snapshot or authorize later writes. */
int contact_batch_preflight_25661166(const ContactBatch *);
int contact_batch_rebuild_25661166(const ContactBatch *, ContactBatchReport *);
/* Can be used directly as RestoreFlowOps.contacts; remaining flow callbacks
   must pin the same owner. Both forward and recovery retire current contacts. */
struct ContactBatchFlow {
    ContactBatch batch;
    ContactBatchReport report;
};
int contact_batch_flow_contacts(void *, uint64_t, int);
#endif
