#ifndef LMU_NATIVE_LOCK_WITNESS_H
#define LMU_NATIVE_LOCK_WITNESS_H
#include <windows.h>
#include <stdint.h>
struct NativeLockWitness {
    uintptr_t owner;
    LONG recursion;
    bool held_by_caller;
};
/* Observe an ALREADY held native lock. Never acquires/releases it, and does not
 * establish that every relevant lifecycle writer uses this same lock. */
NativeLockWitness native_lock_witness(uintptr_t critical_section);
#endif
