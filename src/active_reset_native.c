#include "active_reset_native.h"
#include <string.h>
typedef struct Dispatch {
    ResetNativeRuntime *runtime;
    ResetNativeAdapter adapter;
} Dispatch;
static int capture(void *user, const ResetIdentity *identity) {
    Dispatch *d = user;
    return d->adapter.capture(d->adapter.user, identity);
}
static int restore(void *user, const ResetIdentity *identity) {
    Dispatch *d = user;
    ResetNativeRuntime *r = d->runtime;
    r->plan.count = 0;
    r->scoring = 0;
    memset(&r->bindings, 0, sizeof(r->bindings));
    memset(&r->report, 0, sizeof(r->report));
    if (d->adapter.prepare(d->adapter.user, identity, &r->plan, &r->bindings, &r->scoring))
        return FLOW_REJECTED;
    return restore_joint_native_flow_execute(&r->flow, &r->work, &r->plan, &r->bindings, r->scoring,
                                             (uint32_t)r->control.owner_thread, d->adapter.context,
                                             d->adapter.user, &d->adapter.flow, &r->report);
}
int active_reset_native_step(ResetNativeRuntime *r, const ResetNativeAdapter *adapter,
                             uint64_t thread, uint64_t now, const ResetIdentity *identity,
                             int allowed, const ResetRequest *request) {
    if (!r || !adapter || !adapter->capture || !adapter->prepare || !adapter->context)
        return CONTROL_REJECTED;
    Dispatch dispatch = {r, *adapter};
    ResetActions actions = {&dispatch, capture, restore};
    return active_reset_process(&r->control, thread, now, identity, allowed, request, &actions);
}
