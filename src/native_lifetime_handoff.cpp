#include "native_lifetime_handoff.h"
#include <new>
#include <initializer_list>

struct NativeLifetimeHandoff {
    uintptr_t worker, control, gate;
    DWORD host_thread;
    HANDLE request_event = nullptr, release_event = nullptr, drained_event = nullptr;
    HANDLE thread = nullptr;
    volatile LONG state = static_cast<LONG>(LifetimeHandoffState::Idle);
    volatile LONG stop = 0, cancelled = 0, in_callback = 0;
    ULONGLONG deadline = 0;
};

static LONG value(LifetimeHandoffState state) {
    return static_cast<LONG>(state);
}
static LONG read(const volatile LONG *p) {
    return InterlockedCompareExchange(const_cast<volatile LONG *>(p), 0, 0);
}
static bool host(const NativeLifetimeHandoff *h) {
    return h && h->host_thread == GetCurrentThreadId();
}
static void finish(NativeLifetimeHandoff *h, LifetimeHandoffState state) {
    InterlockedExchange(&h->state, value(state));
    SetEvent(h->drained_event);
}
static DWORD WINAPI serve(void *arg) {
    auto *h = static_cast<NativeLifetimeHandoff *>(arg);
    for (;;) {
        if (WaitForSingleObject(h->request_event, INFINITE) != WAIT_OBJECT_0)
            return 1;
        if (read(&h->stop))
            return 0;
        if (read(&h->cancelled)) {
            finish(h, LifetimeHandoffState::Cancelled);
            continue;
        }
        if (GetTickCount64() >= h->deadline) {
            finish(h, LifetimeHandoffState::Expired);
            continue;
        }
        auto *gate = reinterpret_cast<CRITICAL_SECTION *>(h->gate);
        if (!TryEnterCriticalSection(gate)) {
            finish(h, LifetimeHandoffState::GateBusy);
            continue;
        }
        // The game's control thread releases control before entering this gate.
        // Holding it prevents immediate reacquisition; the physics callback can
        // then acquire control while retaining its existing worker lock.
        InterlockedExchange(&h->state, value(LifetimeHandoffState::Offered));
        const auto now = GetTickCount64();
        if (now < h->deadline && !read(&h->cancelled))
            WaitForSingleObject(h->release_event, static_cast<DWORD>(h->deadline - now));
        const auto terminal =
            read(&h->cancelled) ? LifetimeHandoffState::Cancelled : LifetimeHandoffState::Expired;
        InterlockedCompareExchange(&h->state, value(terminal),
                                   value(LifetimeHandoffState::Offered));
        // Only this thread releases the gate, including timeout and shutdown.
        // A concurrent Claiming callback publishes its own final state.
        LeaveCriticalSection(gate);
        SetEvent(h->drained_event);
        InterlockedCompareExchange(&h->state, value(terminal),
                                   value(LifetimeHandoffState::Offered));
    }
}

NativeLifetimeHandoff *native_handoff_create(uintptr_t worker, uintptr_t control, uintptr_t gate) {
    if (!worker || !control || !gate || worker == control || worker == gate || control == gate)
        return nullptr;
    auto *h = new (std::nothrow) NativeLifetimeHandoff;
    if (!h)
        return nullptr;
    h->worker = worker;
    h->control = control;
    h->gate = gate;
    h->host_thread = GetCurrentThreadId();
    h->request_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    h->release_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    h->drained_event = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    if (h->request_event && h->release_event && h->drained_event)
        h->thread = CreateThread(nullptr, 0, serve, h, 0, nullptr);
    if (!h->thread) {
        native_handoff_destroy(h);
        return nullptr;
    }
    return h;
}

LifetimeHandoffState native_handoff_state(const NativeLifetimeHandoff *h) {
    return h ? static_cast<LifetimeHandoffState>(read(&h->state)) : LifetimeHandoffState::Idle;
}
bool native_handoff_drained(const NativeLifetimeHandoff *h) {
    return h && WaitForSingleObject(h->drained_event, 0) == WAIT_OBJECT_0;
}
bool native_handoff_request(NativeLifetimeHandoff *h, DWORD budget_ms) {
    if (!host(h) || !budget_ms || budget_ms > 50 || read(&h->stop) || !native_handoff_drained(h) ||
        read(&h->in_callback))
        return false;
    const auto state = native_handoff_state(h);
    if (state == LifetimeHandoffState::Claiming || state == LifetimeHandoffState::Offered ||
        state == LifetimeHandoffState::Queued)
        return false;
    ResetEvent(h->drained_event);
    ResetEvent(h->release_event);
    InterlockedExchange(&h->cancelled, 0);
    h->deadline = GetTickCount64() + budget_ms;
    InterlockedExchange(&h->state, value(LifetimeHandoffState::Queued));
    if (!SetEvent(h->request_event)) {
        finish(h, LifetimeHandoffState::Cancelled);
        return false;
    }
    return true;
}
void native_handoff_cancel(NativeLifetimeHandoff *h) {
    if (!host(h))
        return;
    InterlockedExchange(&h->cancelled, 1);
    SetEvent(h->release_event);
}
bool native_handoff_try_pin(NativeLifetimeHandoff *h, NativeLifetimePin *pin) {
    if (!h || !pin || pin->acquired || !native_lock_witness(h->worker).held_by_caller ||
        InterlockedCompareExchange(&h->in_callback, 1, 0) != 0)
        return false;
    if (InterlockedCompareExchange(&h->state, value(LifetimeHandoffState::Claiming),
                                   value(LifetimeHandoffState::Offered)) !=
        value(LifetimeHandoffState::Offered)) {
        InterlockedExchange(&h->in_callback, 0);
        return false;
    }
    bool acquired = false;
    if (!read(&h->cancelled) && GetTickCount64() < h->deadline)
        acquired = native_lifetime_try_acquire(pin, h->worker, h->control);
    const bool cancelled = read(&h->cancelled) != 0;
    const bool expired = GetTickCount64() >= h->deadline || native_handoff_drained(h);
    if (acquired && (cancelled || expired)) {
        native_lifetime_release(pin);
        acquired = false;
    }
    const auto state = acquired    ? LifetimeHandoffState::Acquired
                       : cancelled ? LifetimeHandoffState::Cancelled
                       : expired   ? LifetimeHandoffState::Expired
                                   : LifetimeHandoffState::Offered;
    InterlockedExchange(&h->state, value(state));
    if (state != LifetimeHandoffState::Offered)
        SetEvent(h->release_event);
    else if (native_handoff_drained(h) || GetTickCount64() >= h->deadline)
        InterlockedCompareExchange(&h->state, value(LifetimeHandoffState::Expired),
                                   value(LifetimeHandoffState::Offered));
    InterlockedExchange(&h->in_callback, 0);
    return acquired;
}
bool native_handoff_destroy(NativeLifetimeHandoff *h) {
    if (!host(h) || read(&h->in_callback) ||
        native_handoff_state(h) == LifetimeHandoffState::Claiming)
        return false;
    native_handoff_cancel(h);
    InterlockedExchange(&h->stop, 1);
    if (h->request_event)
        SetEvent(h->request_event);
    if (h->thread && WaitForSingleObject(h->thread, 5000) != WAIT_OBJECT_0)
        return false; // Retain storage/handles if a helper has not actually exited.
    for (HANDLE handle : {h->thread, h->request_event, h->release_event, h->drained_event})
        if (handle)
            CloseHandle(handle);
    delete h;
    return true;
}
