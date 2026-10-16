#include "../../src/native/runtime/native_lifetime_pin.h"
#include <cstdio>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static CRITICAL_SECTION worker_lock, control_lock;
static HANDLE entered, release_control;
static DWORD WINAPI control_thread(void *) {
    EnterCriticalSection(&control_lock);
    SetEvent(entered);
    assert(WaitForSingleObject(release_control, 5000) == WAIT_OBJECT_0);
    LeaveCriticalSection(&control_lock);
    return 0;
}

int main() {
    InitializeCriticalSection(&worker_lock);
    InitializeCriticalSection(&control_lock);
    auto worker = reinterpret_cast<uintptr_t>(&worker_lock);
    auto control = reinterpret_cast<uintptr_t>(&control_lock);
    NativeLifetimePin pin{};
    assert(!native_lifetime_try_acquire(&pin, worker, control));
    EnterCriticalSection(&worker_lock);
    assert(!native_lifetime_try_acquire(&pin, worker, worker));
    entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    release_control = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    assert(entered && release_control);
    HANDLE thread = CreateThread(nullptr, 0, control_thread, nullptr, 0, nullptr);
    assert(thread && WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0);
    assert(!native_lifetime_try_acquire(&pin, worker, control));
    assert(!pin.acquired && native_lock_witness(worker).held_by_caller);
    SetEvent(release_control);
    assert(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    assert(native_lifetime_try_acquire(&pin, worker, control));
    assert(native_lifetime_valid(&pin));
    assert(!native_lifetime_try_acquire(&pin, worker, control));
    assert(native_lifetime_release(&pin));
    assert(!native_lifetime_valid(&pin));
    assert(native_lock_witness(worker).held_by_caller);
    assert(!native_lock_witness(control).held_by_caller);
    // Release only the recursion acquired by this scope.
    EnterCriticalSection(&control_lock);
    assert(native_lifetime_try_acquire(&pin, worker, control));
    assert(native_lock_witness(control).recursion == 2);
    LeaveCriticalSection(&worker_lock);
    assert(!native_lifetime_valid(&pin));
    assert(native_lifetime_release(&pin));
    assert(native_lock_witness(control).recursion == 1);
    LeaveCriticalSection(&control_lock);
    CloseHandle(thread);
    CloseHandle(entered);
    CloseHandle(release_control);
    DeleteCriticalSection(&worker_lock);
    DeleteCriticalSection(&control_lock);
    puts("PASS lifetime scope: existing worker ownership, nonblocking control-lock contention, "
         "distinct locks, nested acquisition refusal, recursion balance and ownership-loss "
         "detection. Fixture locks only.");
}
