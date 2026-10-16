#ifndef LMU_RESTORE_TRANSACTION_H
#define LMU_RESTORE_TRANSACTION_H
#include <stdint.h>
#include <stddef.h>
#define RESTORE_MAX_FIELDS 16384
#define RESTORE_FIELD_BYTES 128
/* Bindings are live-adapter object slots, never pointers stored in a checkpoint.
 * The adapter owns lifetime, field whitelist and physical-thread validation. */
typedef struct RestoreField {
    uint32_t object_slot, field_offset, size, reserved;
    unsigned char expected[RESTORE_FIELD_BYTES], desired[RESTORE_FIELD_BYTES];
} RestoreField;
typedef struct RestorePlan {
    size_t count;
    uint64_t epoch;
    RestoreField fields[RESTORE_MAX_FIELDS];
} RestorePlan;
typedef struct RestoreBackend {
    void *user;
    int (*context)(void *, uint64_t);
    int (*allowed)(void *, uint32_t, uint32_t, uint32_t);
    int (*read)(void *, uint32_t, uint32_t, void *, uint32_t);
    int (*write)(void *, uint32_t, uint32_t, const void *, uint32_t);
} RestoreBackend;
typedef struct RestoreReport {
    size_t attempted, restored;
    int result;
} RestoreReport;
enum RestoreResult {
    RESTORE_OK,
    RESTORE_BAD_PLAN,
    RESTORE_CONTEXT_CHANGED,
    RESTORE_FIELD_FORBIDDEN,
    RESTORE_READ_FAILED,
    RESTORE_STATE_CHANGED,
    RESTORE_WRITE_FAILED,
    RESTORE_VERIFY_FAILED,
    RESTORE_ROLLBACK_FAILED
};
/* Runs only on the designated simulation thread. Preflight reads every field
 * before writing anything. Rollback includes a failed/partially written field.
 * No SDK/shared-memory commands, contacts, scoring or full-coverage guarantee.
 * Keep the plan immutable through this call. No allocations or blocking waits. */
int restore_transaction(const RestorePlan *, const RestoreBackend *, RestoreReport *);
#endif
