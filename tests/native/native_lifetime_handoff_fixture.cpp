#include "../../src/native/runtime/native_lifetime_handoff.h"
#include <cstdio>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static CRITICAL_SECTION worker, control, gate;
static HANDLE holding_control, yield_control, control_resumed;
static NativeLifetimeHandoff *handoff;
static volatile LONG exit_path;
static DWORD WINAPI main_loop(void *) {
    EnterCriticalSection(&control);
    SetEvent(holding_control);
    assert(WaitForSingleObject(yield_control, 5000) == WAIT_OBJECT_0);
    if (exit_path) {
        // Adversarial teardown: holds control while draining the worker.
        EnterCriticalSection(&worker);
        LeaveCriticalSection(&worker);
    }
    LeaveCriticalSection(&control);
    EnterCriticalSection(&gate);
    LeaveCriticalSection(&gate);
    EnterCriticalSection(&control);
    SetEvent(control_resumed);
    LeaveCriticalSection(&control);
    return 0;
}
static void until_offered() {
    const auto deadline = GetTickCount64() + 2000;
    while (native_handoff_state(handoff) == LifetimeHandoffState::Queued &&
           GetTickCount64() < deadline)
        SwitchToThread();
    assert(native_handoff_state(handoff) == LifetimeHandoffState::Offered);
}
static void until_drained() {
    const auto deadline = GetTickCount64() + 2000;
    while (!native_handoff_drained(handoff) && GetTickCount64() < deadline)
        Sleep(1);
    assert(native_handoff_drained(handoff));
}
static HANDLE start_main() {
    ResetEvent(holding_control);
    ResetEvent(yield_control);
    ResetEvent(control_resumed);
    HANDLE thread = CreateThread(nullptr, 0, main_loop, nullptr, 0, nullptr);
    assert(thread && WaitForSingleObject(holding_control, 5000) == WAIT_OBJECT_0);
    return thread;
}
static void join(HANDLE thread) {
    assert(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    CloseHandle(thread);
}
int main() {
    InitializeCriticalSection(&worker);
    InitializeCriticalSection(&control);
    InitializeCriticalSection(&gate);
    const auto w = reinterpret_cast<uintptr_t>(&worker);
    const auto c = reinterpret_cast<uintptr_t>(&control);
    const auto g = reinterpret_cast<uintptr_t>(&gate);
    assert(!native_handoff_create(w, c, c));
    handoff = native_handoff_create(w, c, g);
    assert(handoff);
    assert(!native_handoff_request(handoff, 0));
    assert(!native_handoff_request(handoff, 51));
    holding_control = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    yield_control = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    control_resumed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    assert(holding_control && yield_control && control_resumed);

    // A real control-thread handoff. No test thread releases another's lock.
    HANDLE thread = start_main();
    assert(native_handoff_request(handoff, 50));
    until_offered();
    NativeLifetimePin pin{};
    assert(!native_handoff_try_pin(handoff, &pin)); // Missing worker scope.
    assert(!native_handoff_request(handoff, 50));   // No overlapping request.
    EnterCriticalSection(&worker);
    assert(!native_handoff_try_pin(handoff, &pin)); // Control still busy.
    SetEvent(yield_control);
    const auto deadline = GetTickCount64() + 1000;
    while (!native_handoff_try_pin(handoff, &pin) && GetTickCount64() < deadline)
        SwitchToThread();
    assert(pin.acquired && native_lifetime_valid(&pin));
    assert(native_handoff_state(handoff) == LifetimeHandoffState::Acquired);
    until_drained();
    assert(WaitForSingleObject(control_resumed, 0) == WAIT_TIMEOUT);
    assert(TryEnterCriticalSection(&gate)); // Helper has released its gate.
    LeaveCriticalSection(&gate);
    assert(native_lifetime_release(&pin));
    LeaveCriticalSection(&worker);
    join(thread);

    // An occupied gate is refused without waiting for its owner.
    EnterCriticalSection(&gate);
    assert(native_handoff_request(handoff, 50));
    until_drained();
    assert(native_handoff_state(handoff) == LifetimeHandoffState::GateBusy);
    LeaveCriticalSection(&gate);

    // No physics callback arrives: timeout still releases the admission gate.
    assert(native_handoff_request(handoff, 30));
    until_drained();
    assert(native_handoff_state(handoff) == LifetimeHandoffState::Expired);
    assert(TryEnterCriticalSection(&gate));
    LeaveCriticalSection(&gate);
    assert(native_handoff_request(handoff, 50));
    until_offered();
    native_handoff_cancel(handoff);
    until_drained();
    assert(native_handoff_state(handoff) == LifetimeHandoffState::Cancelled);
    EnterCriticalSection(&worker);
    assert(!native_handoff_try_pin(handoff, &pin));
    LeaveCriticalSection(&worker);

    // Opposite lock order during exit must not deadlock the worker callback.
    exit_path = 1;
    thread = start_main();
    EnterCriticalSection(&worker);
    assert(native_handoff_request(handoff, 30));
    until_offered();
    SetEvent(yield_control);
    unsigned refusals = 0;
    while (!native_handoff_drained(handoff)) {
        assert(!native_handoff_try_pin(handoff, &pin));
        ++refusals;
        SwitchToThread();
    }
    assert(refusals && native_lock_witness(w).held_by_caller);
    assert(!pin.acquired);
    assert(native_handoff_state(handoff) == LifetimeHandoffState::Expired);
    LeaveCriticalSection(&worker);
    join(thread);

    // Shutdown cancels an offered request and joins the actual gate owner.
    assert(native_handoff_request(handoff, 50));
    until_offered();
    assert(native_handoff_shutdown(handoff));
    assert(native_handoff_drained(handoff));
    assert(!native_handoff_request(handoff, 50));
    EnterCriticalSection(&worker);
    assert(!native_handoff_try_pin(handoff, &pin));
    LeaveCriticalSection(&worker);
    assert(native_handoff_destroy(handoff));
    assert(TryEnterCriticalSection(&gate));
    LeaveCriticalSection(&gate);
    CloseHandle(holding_control);
    CloseHandle(yield_control);
    CloseHandle(control_resumed);
    DeleteCriticalSection(&worker);
    DeleteCriticalSection(&control);
    DeleteCriticalSection(&gate);
    puts("PASS asynchronous lifetime admission: control handoff, busy gate, missing worker, "
         "overlap refusal, expiration without callbacks, cancellation, inverted exit lock "
         "order and shutdown. Actual Win32 threads and fixture locks only.");
}
