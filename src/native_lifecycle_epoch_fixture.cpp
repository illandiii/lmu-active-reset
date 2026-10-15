#include "native_lifecycle_epoch.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>

static NativeLifecycleEpoch epoch;
static HANDLE attempted, entered, resume;
static DWORD WINAPI mutate(void *) {
    SetEvent(attempted);
    native_lifecycle_mutation_enter(&epoch);
    SetEvent(entered);
    assert(WaitForSingleObject(resume, 5000) == WAIT_OBJECT_0);
    native_lifecycle_mutation_leave(&epoch);
    return 0;
}
int main() {
    native_lifecycle_init(&epoch);
    assert(!native_lifecycle_changed(&epoch, 1));
    NativeLifecyclePin pin{};
    assert(native_lifecycle_try_pin(&epoch, 1, &pin));
    assert(native_lifecycle_valid(&pin));
    assert(!native_lifecycle_try_pin(&epoch, 1, &pin));
    attempted = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    resume = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE thread = CreateThread(nullptr, 0, mutate, nullptr, 0, nullptr);
    assert(thread && WaitForSingleObject(attempted, 5000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(entered, 10) == WAIT_TIMEOUT);
    assert(native_lifecycle_release(&pin));
    assert(WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0);
    assert(!native_lifecycle_try_pin(&epoch, 1, &pin));
    SetEvent(resume);
    assert(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    assert(native_lifecycle_changed(&epoch, 1));
    assert(!native_lifecycle_changed(&epoch, 2));
    assert(!native_lifecycle_try_pin(&epoch, 1, &pin));
    assert(native_lifecycle_try_pin(&epoch, 2, &pin));
    // Reentrant teardown invalidates even though the lock remains recursive.
    native_lifecycle_mutation_enter(&epoch);
    native_lifecycle_mutation_enter(&epoch);
    assert(!native_lifecycle_valid(&pin));
    NativeLifecyclePin nested{};
    assert(!native_lifecycle_try_pin(&epoch, 4, &nested));
    native_lifecycle_mutation_leave(&epoch);
    native_lifecycle_mutation_leave(&epoch);
    assert(!native_lifecycle_valid(&pin));
    assert(native_lifecycle_release(&pin));
    assert(!native_lifecycle_try_pin(&epoch, 2, &pin));
    assert(native_lifecycle_try_pin(&epoch, 4, &pin));
    assert(native_lifecycle_release(&pin));
    epoch.generation = std::numeric_limits<uint64_t>::max();
    native_lifecycle_mutation_enter(&epoch);
    native_lifecycle_mutation_leave(&epoch);
    assert(!epoch.generation && !native_lifecycle_try_pin(&epoch, 1, &pin));
    for (HANDLE handle : {thread, attempted, entered, resume})
        CloseHandle(handle);
    DeleteCriticalSection(&epoch.lock);
    puts("PASS lifecycle epoch: concurrent teardown exclusion, no waiting in pin acquisition, "
         "same-address invalidation, nested mutations, reentrant loss and generation saturation.");
}
