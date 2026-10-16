#include "native_lock_witness.h"
#include <cstddef>
static_assert(sizeof(CRITICAL_SECTION) == 40 && offsetof(CRITICAL_SECTION, OwningThread) == 16,
              "Win64 lock ABI");
NativeLockWitness native_lock_witness(uintptr_t address) {
    CRITICAL_SECTION copy{};
    SIZE_T got = 0;
    if (address < 0x10000 || address > 0x800000000000ULL - sizeof(copy) ||
        !ReadProcessMemory(GetCurrentProcess(), (void *)address, &copy, sizeof(copy), &got) ||
        got != sizeof(copy))
        return {};
    auto owner = (uintptr_t)copy.OwningThread;
    return {owner, copy.RecursionCount, owner == GetCurrentThreadId() && copy.RecursionCount > 0};
}
