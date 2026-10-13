#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "contact_batch_25661166.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <initializer_list>
static unsigned char tire[4][0x1200], pool[4][4][0x110], surface[4][2][0x58];
static uintptr_t active[4][8];
static uint32_t bits[4];
static uint64_t calls;
static int stationary = 1, abi_errors = 0;
template <class T> static void put(void *p, const T &value) {
    memcpy(p, &value, sizeof(value));
}
template <class T> static T get(void *p) {
    T value;
    memcpy(&value, p, sizeof(value));
    return value;
}
extern "C" __declspec(dllexport) int WINAPI FixtureWheel(void *p, uintptr_t *player) {
    *player = (uintptr_t)tire;
    for (int i = 0; i < 4; i++)
        if (p == tire[i])
            return i;
    return -1;
}
extern "C" __declspec(dllexport) int WINAPI FixtureStationary() {
    return stationary;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureRefresh(void *p, double dt,
                                                                                 double horizon,
                                                                                 void *world,
                                                                                 double angular) {
    ++calls;
    if (dt != .001 || horizon != .003 || world != (void *)0x12345678 || angular != 2.5) {
        ++abi_errors;
        return;
    }
    uintptr_t player;
    int w = FixtureWheel(p, &player);
    if (w < 0)
        return;
    size_t count = (get<uintptr_t>(tire[w] + 0x9f8) - (uintptr_t)active[w]) / 8;
    for (size_t i = 0; i < count;) {
        auto *n = (unsigned char *)active[w][i];
        double d = get<double>(n + 0xd0);
        if (d <= 0) {
            n[0x60] = n[0x67] = 0;
            active[w][i] = active[w][--count];
        } else
            ++i;
    }
    for (int i = 0; i < 3; i++) {
        auto *n = pool[w][i];
        if (!n[0x60]) {
            n[0x60] = 1;
            active[w][count++] = (uintptr_t)n;
        }
        double geometry[5] = {.02 + i * .001, 0, 1, 0, 3};
        put(n + 0xd0, geometry);
    }
    put(tire[w] + 0x9f8, (uintptr_t)active[w] + count * 8);
}
static void init() {
    for (int w = 0; w < 4; w++) {
        put(tire[w] + 0x318, (uintptr_t)pool[w]);
        uintptr_t v[3] = {(uintptr_t)active[w], (uintptr_t)active[w] + 24,
                          (uintptr_t)active[w] + 64};
        put(tire[w] + 0x9f0, v);
        uintptr_t sv[3] = {(uintptr_t)surface[w], (uintptr_t)surface[w] + 0xb0,
                           (uintptr_t)surface[w] + 0xb0};
        put(tire[w] + 0xa60, sv);
        uintptr_t bv[3] = {(uintptr_t)&bits[w], (uintptr_t)&bits[w] + 4, (uintptr_t)&bits[w] + 4};
        put(tire[w] + 0xa78, bv);
        bits[w] = 3;
        for (int i = 0; i < 2; i++)
            put(surface[w][i], (uintptr_t)(0x20000 + i * 0x1000));
        for (int i = 0; i < 3; i++) {
            active[w][i] = (uintptr_t)pool[w][i];
            pool[w][i][0x60] = pool[w][i][0x67] = 1;
            put(pool[w][i] + 0x62, (uint16_t)(i % 2));
            put(pool[w][i] + 0xf8, (uintptr_t)(0x20000 + (i % 2) * 0x1000));
            double geo[5] = {.02, 0, 1, 0, 3};
            put(pool[w][i] + 0xd0, geo);
        }
    }
}

static int alive = 1;
static int owner(void *, uint64_t epoch) {
    return alive && epoch == 42;
}
static bool leave_cold = false, move_pose = false, lose_owner = false;
static void WINAPI refresh_test(void *p, double d, double h, void *world, double a) {
    FixtureRefresh(p, d, h, world, a);
    if (leave_cold) {
        uintptr_t root = get<uintptr_t>((unsigned char *)p + 0x318);
        ((unsigned char *)root)[0x67] = 1;
    }
    if (move_pose) {
        double x = 1;
        put((unsigned char *)p + 0xb00, x);
    }
    if (lose_owner)
        alive = 0;
}
int batch_tests() {
    ContactBatch b{};
    b.world = (void *)0x12345678;
    b.owner_thread = GetCurrentThreadId();
    b.epoch = 42;
    b.owner_alive = owner;
    b.refresh = refresh_test;
    for (int w = 0; w < 4; w++) {
        b.tire[w] = (uintptr_t)tire[w];
        b.dt[w] = .001;
        b.horizon[w] = .003;
        b.angular[w] = 2.5;
    }
    ContactBatchReport r{};
    init();
    assert(FixtureStationary());
    assert(contact_batch_rebuild_25661166(&b, &r) && r.completed_mask == 15 &&
           r.write_attempts == 12 && r.refresh_calls == 4 && !abi_errors);
    init();
    double bad = NAN;
    put(pool[3][2] + 0xd8, bad);
    assert(!contact_batch_rebuild_25661166(&b, &r) && !r.write_attempts && !r.refresh_calls);
    init();
    b.tire[3] = b.tire[0];
    assert(!contact_batch_rebuild_25661166(&b, &r) && !r.write_attempts);
    b.tire[3] = (uintptr_t)tire[3];
    b.owner_thread++;
    assert(!contact_batch_rebuild_25661166(&b, &r) && !r.write_attempts);
    b.owner_thread = GetCurrentThreadId();
    alive = 0;
    assert(!contact_batch_rebuild_25661166(&b, &r) && !r.write_attempts);
    alive = 1;
    init();
    leave_cold = true;
    assert(!contact_batch_rebuild_25661166(&b, &r) && r.write_attempts == 3 && !r.completed_mask);
    leave_cold = false;
    init();
    move_pose = true;
    assert(!contact_batch_rebuild_25661166(&b, &r) && r.refresh_calls == 1);
    move_pose = false;
    init();
    lose_owner = true;
    assert(!contact_batch_rebuild_25661166(&b, &r) && r.refresh_calls == 1);
    lose_owner = false;
    alive = 1;
    puts("PASS four-wheel contact batch: full preflight, late malformed node, duplicate wheel, "
         "owner thread/lifetime, cold-history flags, pose invariance and post-call owner loss. "
         "Owned-memory model only.");
    return 0;
}

extern "C" {
#include "restore_flow.h"
#include "scoring_plan_25661166.h"
}
static RestoreFlow flow_state;
static RestoreFlowWork flow_work;
static RestorePlan flow_plan;
static unsigned char score[0x27400];
static double pose = 1;
static ContactBatch flow_batch;
static ContactBatchReport batch_report;
static int held = 0, fail_target = 0, fail_original = 0, resumed = -1;
static int flow_owner(void *, uint64_t epoch) {
    return held && alive && epoch == 42;
}
static int flow_enter(void *, uint64_t epoch) {
    held = epoch == 42;
    return held;
}
static void flow_leave(void *, int resume) {
    resumed = resume;
    held = 0;
}
static void flow_quarantine(void *, int) {
    assert(held);
}
static int flow_allowed(void *, uint32_t slot, uint32_t off, uint32_t n) {
    return (slot == 0 && off == 0 && n == 8) ||
           (slot == 2700 && scoring_field_allowed_25661166(0, off, n));
}
static int flow_read(void *u, uint32_t slot, uint32_t off, void *out, uint32_t n) {
    if (!flow_allowed(u, slot, off, n))
        return 0;
    memcpy(out, slot == 0 ? (void *)&pose : (void *)(score + off), n);
    return 1;
}
static int flow_write(void *u, uint32_t slot, uint32_t off, const void *in, uint32_t n) {
    if (!flow_allowed(u, slot, off, n))
        return 0;
    memcpy(slot == 0 ? (void *)&pose : (void *)(score + off), in, n);
    return 1;
}
static int flow_contacts(void *, uint64_t epoch, int original) {
    assert(flow_owner(nullptr, epoch) && pose == (original ? 1 : 9));
    ContactBatchFlow callback{flow_batch, {}};
    int rebuilt = contact_batch_flow_contacts(&callback, epoch, original);
    batch_report = callback.report;
    if (!rebuilt)
        return 0;
    return !(original ? fail_original : fail_target);
}
static void flow_init() {
    init();
    alive = 1;
    pose = 1;
    held = fail_target = fail_original = 0;
    resumed = -1;
    flow_state = {};
    flow_plan.epoch = 42;
    flow_plan.count = 4;
    auto &f = flow_plan.fields[0];
    f = {};
    f.size = 8;
    double next = 9;
    memcpy(f.expected, &pose, 8);
    memcpy(f.desired, &next, 8);
    unsigned offsets[3] = {0x160, 0x27290, 0x27360};
    int32_t flag = 2;
    for (int i = 0; i < 3; i++) {
        auto &sf = flow_plan.fields[i + 1];
        sf = {};
        sf.object_slot = 2700;
        sf.field_offset = offsets[i];
        sf.size = 4;
        memcpy(sf.expected, &flag, 4);
        memcpy(score + offsets[i], &flag, 4);
    }
    flow_batch = {};
    flow_batch.world = (void *)0x12345678;
    flow_batch.owner_thread = GetCurrentThreadId();
    flow_batch.epoch = 42;
    flow_batch.owner_alive = flow_owner;
    flow_batch.refresh = refresh_test;
    for (int w = 0; w < 4; w++) {
        flow_batch.tire[w] = (uintptr_t)tire[w];
        flow_batch.dt[w] = .001;
        flow_batch.horizon[w] = .003;
        flow_batch.angular[w] = 2.5;
    }
}
static int flow_run() {
    RestoreBackend backend{nullptr, flow_owner, flow_allowed, flow_read, flow_write};
    RestoreFlowOps ops{nullptr, flow_enter, flow_owner, flow_contacts, flow_leave, flow_quarantine};
    RestoreFlowReport report{};
    return restore_flow_execute(&flow_state, &flow_work, &flow_plan, &backend, &ops, &report);
}
int main() {
    batch_tests();
    flow_init();
    assert(flow_run() == FLOW_OK && pose == 9 && resumed == 1 && batch_report.completed_mask == 15);
    flow_init();
    fail_target = 1;
    assert(flow_run() == FLOW_COMPENSATED && pose == 1 && resumed == 1 &&
           batch_report.completed_mask == 15);
    for (unsigned off : {0x160u, 0x27290u, 0x27360u})
        assert(get<int32_t>(score + off) == 2);
    flow_init();
    fail_target = fail_original = 1;
    assert(flow_run() == FLOW_QUARANTINED && resumed == 0 && flow_state.quarantined);
    puts("PASS restore flow with node-layout contact batch: commit, four-wheel rebuild, full "
         "compensation and failed-recovery lockout. Native refresh is a fixture model.");
    return 0;
}
