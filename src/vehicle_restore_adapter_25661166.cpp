#include "vehicle_restore_adapter_25661166.h"
#include "vehicle_checkpoint_25661166.h"
#include "native_lock_witness.h"
#include <cmath>
#include <cstring>
#include <new>

struct VehicleRestoreAdapter {
    VehicleRestoreHost host;
    VehicleRestoreContext context;
    ResetIdentity saved_identity;
    uint64_t saved_epoch;
    bool bound, occupied, held, quarantined;
    SnapshotFrame saved, current;
    RestoreBindings bindings;
    uintptr_t scoring;
    VehicleCheckpointWorkspace *workspace;
    ContactBatchFlow contacts;
#ifdef LMU_VEHICLE_ADAPTER_FIXTURE
    VehicleFixtureReader reader = vehicle_read_25661166;
    uintptr_t fixture_scoring;
#endif
};

static int owner(void *user, uint64_t epoch) {
    auto *adapter = static_cast<VehicleRestoreAdapter *>(user);
    return adapter && adapter->bound && !adapter->quarantined && epoch == adapter->context.epoch &&
           GetCurrentThreadId() == adapter->host.owner_thread &&
           native_lock_witness(adapter->host.native_lock).held_by_caller &&
           native_lock_witness(adapter->host.control_lock).held_by_caller &&
           adapter->host.pinned(adapter->host.user, &adapter->context);
}

static bool same_identity(const ResetIdentity &a, const ResetIdentity &b) {
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

static bool read_current(VehicleRestoreAdapter *adapter) {
    const auto &context = adapter->context;
    if (!owner(adapter, context.epoch))
        return false;
#ifdef LMU_VEHICLE_ADAPTER_FIXTURE
    const auto reader = adapter->reader;
#else
    const auto reader = vehicle_read_25661166;
#endif
    return reader(adapter->host.module_base, context.physics, context.epoch,
                  context.simulation_time, &adapter->current, &adapter->bindings) &&
           owner(adapter, context.epoch);
}

static int capture(void *user, const ResetIdentity *identity) {
    auto *adapter = static_cast<VehicleRestoreAdapter *>(user);
    if (!identity || !same_identity(*identity, adapter->context.identity) ||
        !read_current(adapter) ||
        vehicle_checkpoint_validate(adapter->workspace, &adapter->current) ||
        !owner(adapter, adapter->context.epoch))
        return 1;
    // Publish only after every subsystem validates. A failed replacement keeps
    // the previous checkpoint and identity intact.
    adapter->saved = adapter->current;
    adapter->saved_identity = *identity;
    adapter->saved_epoch = adapter->context.epoch;
    adapter->occupied = true;
    return 0;
}

static int allowed(void *user, uint32_t slot, uint32_t offset, uint32_t size) {
    auto *adapter = static_cast<VehicleRestoreAdapter *>(user);
    return owner(adapter, adapter->context.epoch) &&
           restore_binding_field(&adapter->bindings, slot, offset, size);
}

static int read(void *user, uint32_t slot, uint32_t offset, void *value, uint32_t size) {
    auto *adapter = static_cast<VehicleRestoreAdapter *>(user);
    if (!allowed(user, slot, offset, size))
        return 0;
    SIZE_T count = 0;
    return ReadProcessMemory(GetCurrentProcess(),
                             reinterpret_cast<void *>(adapter->bindings.address[slot] + offset),
                             value, size, &count) &&
           count == size;
}

template <typename T> static bool field(uintptr_t address, T &value) {
    SIZE_T count = 0;
    return address >= 0x10000 &&
           ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(address), &value,
                             sizeof(value), &count) &&
           count == sizeof(value);
}

static bool prepare_contacts(VehicleRestoreAdapter *adapter) {
    auto &batch = adapter->contacts.batch;
    batch = {};
    adapter->contacts.report = {};
    batch.owner_thread = adapter->host.owner_thread;
    batch.epoch = adapter->context.epoch;
    batch.user = adapter;
    batch.owner_alive = owner;
    batch.refresh = adapter->host.refresh;
    batch.world = reinterpret_cast<void *>(adapter->host.module_base + 0x1aa4090);
    const auto now = GetTickCount64();
    for (unsigned i = 0; i < 4; ++i) {
        const auto &sample = adapter->context.contacts[i];
        if (sample.tire != adapter->bindings.address[96 + i] || sample.epoch != batch.epoch ||
            sample.thread != batch.owner_thread ||
            sample.world != reinterpret_cast<uintptr_t>(batch.world) || sample.tick_ms > now ||
            now - sample.tick_ms > 100)
            return false;
        batch.tire[i] = sample.tire;
        batch.dt[i] = sample.dt;
        batch.horizon[i] = sample.horizon;
        batch.angular[i] = sample.angular;
    }
    return contact_batch_preflight_25661166(&batch) != 0;
}

static int prepare(void *user, const ResetIdentity *identity, RestorePlan *plan,
                   RestoreBindings *bindings, uintptr_t *scoring) {
    auto *adapter = static_cast<VehicleRestoreAdapter *>(user);
    plan->count = 0;
    *bindings = {};
    *scoring = 0;
    if (!identity || !adapter->occupied || adapter->saved_epoch != adapter->context.epoch ||
        !same_identity(*identity, adapter->saved_identity) ||
        !same_identity(*identity, adapter->context.identity) || !read_current(adapter))
        return 1;
    adapter->scoring =
        vehicle_scoring_25661166(adapter->host.module_base, adapter->context.physics);
#ifdef LMU_VEHICLE_ADAPTER_FIXTURE
    adapter->scoring = adapter->fixture_scoring;
#endif
    LapGuardFlags flags{};
    if (!adapter->scoring || !field(adapter->scoring + 0x160, flags.current) ||
        !field(adapter->scoring + 0x27290, flags.remembered) ||
        !field(adapter->scoring + 0x27360, flags.last_computed) ||
        !field(adapter->scoring + 0x164, flags.next_timed) ||
        !field(adapter->scoring + 0x272b8, flags.off_track) || !prepare_contacts(adapter))
        return 1;
    RestoreBackend backend{adapter, owner, allowed, read, nullptr};
    if (vehicle_checkpoint_prepare(adapter->workspace, &adapter->saved, &adapter->current,
                                   &adapter->bindings, &backend, &flags, plan) ||
        !owner(adapter, adapter->context.epoch)) {
        plan->count = 0;
        return 1;
    }
    *bindings = adapter->bindings;
    *scoring = adapter->scoring;
    return 0;
}

static int enter(void *user, uint64_t epoch) {
    auto *adapter = static_cast<VehicleRestoreAdapter *>(user);
    if (adapter->held || !owner(user, epoch))
        return 0;
    adapter->held = true;
    return 1;
}

static int contacts(void *user, uint64_t epoch, int original) {
    auto *adapter = static_cast<VehicleRestoreAdapter *>(user);
    return adapter->held && owner(user, epoch) &&
           contact_batch_flow_contacts(&adapter->contacts, epoch, original);
}

static void leave(void *user, int) {
    static_cast<VehicleRestoreAdapter *>(user)->held = false;
}

static void quarantine(void *user, int) {
    static_cast<VehicleRestoreAdapter *>(user)->quarantined = true;
}

VehicleRestoreAdapter *vehicle_restore_adapter_create(const VehicleRestoreHost *host) {
    if (!host || !host->module_base || !host->native_lock || !host->control_lock ||
        host->native_lock == host->control_lock || !host->owner_thread || !host->pinned ||
        !host->refresh)
        return nullptr;
    auto *adapter = new (std::nothrow) VehicleRestoreAdapter{};
    if (!adapter)
        return nullptr;
    adapter->host = *host;
    adapter->workspace = vehicle_checkpoint_workspace_create();
    if (!adapter->workspace) {
        delete adapter;
        return nullptr;
    }
    return adapter;
}

void vehicle_restore_adapter_destroy(VehicleRestoreAdapter *adapter) {
    if (!adapter || adapter->held)
        return;
    vehicle_checkpoint_workspace_destroy(adapter->workspace);
    delete adapter;
}

bool vehicle_restore_adapter_bind(VehicleRestoreAdapter *adapter,
                                  const VehicleRestoreContext *context) {
    if (!adapter || adapter->held || adapter->quarantined)
        return false;
    adapter->bound = false;
    if (!context || !context->epoch || !context->physics || !context->identity.process ||
        !context->identity.session || !context->identity.vehicle ||
        !context->identity.configuration || !std::isfinite(context->simulation_time))
        return false;
    if (adapter->occupied && (context->epoch != adapter->saved_epoch ||
                              !same_identity(context->identity, adapter->saved_identity)))
        adapter->occupied = false;
    adapter->context = *context;
    adapter->bound = true;
    if (!owner(adapter, context->epoch)) {
        adapter->bound = false;
        return false;
    }
    return true;
}

ResetNativeAdapter vehicle_restore_adapter_ops(VehicleRestoreAdapter *adapter) {
    return {adapter, capture, prepare, owner, {adapter, enter, owner, contacts, leave, quarantine}};
}

ContactBatchReport vehicle_restore_adapter_contact_report(const VehicleRestoreAdapter *adapter) {
    return adapter ? adapter->contacts.report : ContactBatchReport{};
}

#ifdef LMU_VEHICLE_ADAPTER_FIXTURE
void vehicle_restore_adapter_fixture_reader(VehicleRestoreAdapter *adapter,
                                            VehicleFixtureReader reader, uintptr_t scoring) {
    adapter->reader = reader;
    adapter->fixture_scoring = scoring;
}
#endif
