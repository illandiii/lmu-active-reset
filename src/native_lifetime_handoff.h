#ifndef LMU_NATIVE_LIFETIME_HANDOFF_H
#define LMU_NATIVE_LIFETIME_HANDOFF_H
#include "native_lifetime_pin.h"

struct NativeLifetimeHandoff;
enum class LifetimeHandoffState : LONG {
    Idle,
    Queued,
    Offered,
    Claiming,
    Acquired,
    Expired,
    Cancelled,
    GateBusy
};

// Create/request/cancel/destroy belong to one host thread, outside physics.
// The helper owns only the admission gate. It never waits for the worker or
// control lock. Physics may try the offered control lock without waiting.
NativeLifetimeHandoff *native_handoff_create(uintptr_t worker, uintptr_t control, uintptr_t gate);
bool native_handoff_request(NativeLifetimeHandoff *, DWORD budget_ms);
void native_handoff_cancel(NativeLifetimeHandoff *);
LifetimeHandoffState native_handoff_state(const NativeLifetimeHandoff *);
bool native_handoff_drained(const NativeLifetimeHandoff *);
bool native_handoff_try_pin(NativeLifetimeHandoff *, NativeLifetimePin *);
// Call only after detaching/quiescing all physics callbacks using this object.
bool native_handoff_destroy(NativeLifetimeHandoff *);
#endif
