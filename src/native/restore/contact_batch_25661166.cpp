#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstring>
#include <cmath>
#include "contact_batch_25661166.h"
static constexpr size_t CONTACT_MAX = 512;
// Node layout/validation extracted from the build-pinned contact v2 observer.
static bool self_read(uintptr_t p, void *out, size_t size) {
    SIZE_T got = 0;
    return p >= 0x10000 && p < 0x800000000000ULL &&
           ReadProcessMemory(GetCurrentProcess(), (void *)p, out, size, &got) && got == size;
}
template <class T> static bool field(uintptr_t p, T &out) {
    return self_read(p, &out, sizeof(out));
}
struct Node {
    uintptr_t address;
    uint32_t index;
    uint16_t surface_index;
    uint8_t active, aux;
    double depth, normal[3], plane;
};
struct Snapshot {
    Node nodes[CONTACT_MAX];
    uint64_t count, surfaces, bad, positive;
    double min_depth, max_depth, normal_min, normal_max, position[3];
};
static bool vector_size(const uintptr_t *v, size_t stride, size_t limit, size_t &count) {
    if (v[0] == 0 && v[1] == 0 && v[2] == 0) {
        count = 0;
        return true;
    }
    if (v[0] < 0x10000 || v[1] < v[0] || v[2] < v[1] || (v[1] - v[0]) % stride ||
        (v[2] - v[0]) % stride)
        return false;
    count = (v[1] - v[0]) / stride;
    return count <= limit;
}
static bool snapshot(uintptr_t tire, Snapshot &s) {
    s.count = s.surfaces = s.bad = s.positive = 0;
    s.min_depth = s.normal_min = 1e300;
    s.max_depth = -1e300;
    s.normal_max = 0;
    uintptr_t active[3], surfaces[3], used[3], pool = 0;
    size_t count = 0, sc = 0, uc = 0;
    if (!field(tire + 0x318, pool) || !field(tire + 0x9f0, active) ||
        !field(tire + 0xa60, surfaces) || !field(tire + 0xa78, used) ||
        !vector_size(active, 8, CONTACT_MAX, count) || !vector_size(surfaces, 0x58, 1024, sc) ||
        !vector_size(used, 4, 1024, uc) || !field(tire + 0xb00, s.position))
        return false;
    for (double x : s.position)
        if (!std::isfinite(x))
            return false;
    s.count = count;
    s.surfaces = sc;
    uintptr_t nodes[CONTACT_MAX];
    uint32_t bits[1024];
    if (count && !self_read(active[0], nodes, count * 8))
        return false;
    if (uc && !self_read(used[0], bits, uc * 4))
        return false;
    for (size_t i = 0; i < count; i++) {
        Node &n = s.nodes[i];
        n.address = nodes[i];
        if (pool < 0x10000 || nodes[i] < pool || (nodes[i] - pool) % 0x110 ||
            (nodes[i] - pool) / 0x110 >= 32768)
            return false;
        n.index = (uint32_t)((nodes[i] - pool) / 0x110);
        for (size_t j = 0; j < i; j++)
            if (nodes[j] == nodes[i])
                return false;
        unsigned char flags[8];
        double geo[5];
        uintptr_t surface = 0, cached = 0;
        if (!field(nodes[i] + 0x60, flags) || !field(nodes[i] + 0xd0, geo) ||
            !field(nodes[i] + 0xf8, surface))
            return false;
        memcpy(&n.surface_index, flags + 2, 2);
        n.active = flags[0];
        n.aux = flags[7];
        n.depth = geo[0];
        memcpy(n.normal, geo + 1, 24);
        n.plane = geo[4];
        for (double x : geo)
            if (!std::isfinite(x))
                return false;
        double norm = std::sqrt(geo[1] * geo[1] + geo[2] * geo[2] + geo[3] * geo[3]);
        if (n.surface_index >= sc || n.surface_index / 32 >= uc)
            return false;
        if (!field(surfaces[0] + n.surface_index * 0x58, cached))
            return false;
        if (n.active != 1 || !surface || surface != cached ||
            !(bits[n.surface_index / 32] & (1u << (n.surface_index % 32))) || norm < .999 ||
            norm > 1.001)
            ++s.bad;
        if (n.depth > 0)
            ++s.positive;
        if (n.depth < s.min_depth)
            s.min_depth = n.depth;
        if (n.depth > s.max_depth)
            s.max_depth = n.depth;
        if (norm < s.normal_min)
            s.normal_min = norm;
        if (norm > s.normal_max)
            s.normal_max = norm;
    }
    if (!count)
        s.min_depth = s.max_depth = s.normal_min = s.normal_max = 0;
    return true;
}
static bool writable_depths(const Snapshot &s) {
    for (size_t i = 0; i < s.count; i++) {
        MEMORY_BASIC_INFORMATION m{};
        uintptr_t p = s.nodes[i].address + 0xd0;
        if (!VirtualQuery((void *)p, &m, sizeof(m)) || m.State != MEM_COMMIT ||
            m.Protect & (PAGE_GUARD | PAGE_NOACCESS) ||
            !(m.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) ||
            p + 8 > (uintptr_t)m.BaseAddress + m.RegionSize)
            return false;
    }
    return true;
}

static int preflight(const ContactBatch &b, Snapshot *before) {
    // This function requires an externally pinned, non-advancing owner boundary.
    // No process discovery, injection, context override or pointer re-resolution.
    for (int w = 0; w < 4; w++) {
        if (!b.owner_alive(b.user, b.epoch) || !b.tire[w] || !std::isfinite(b.dt[w]) ||
            b.dt[w] <= 0 || b.dt[w] > .01 || !std::isfinite(b.horizon[w]) ||
            b.horizon[w] < b.dt[w] || b.horizon[w] > .02 || !std::isfinite(b.angular[w]) ||
            b.angular[w] < 0 || b.angular[w] >= 5000)
            return 0;
        for (int j = 0; j < w; j++)
            if (b.tire[j] == b.tire[w])
                return 0;
        if (!snapshot(b.tire[w], before[w]) || before[w].bad || !before[w].count ||
            !before[w].positive || !writable_depths(before[w]))
            return 0;
        for (size_t i = 0; i < before[w].count; i++)
            for (int j = 0; j < w; j++)
                for (size_t k = 0; k < before[j].count; k++)
                    if (before[w].nodes[i].address == before[j].nodes[k].address)
                        return 0;
    }
    return b.owner_alive(b.user, b.epoch) != 0;
}
int contact_batch_preflight_25661166(const ContactBatch *input) {
    if (!input || !input->refresh || !input->owner_alive || !input->epoch || !input->world ||
        input->owner_thread != GetCurrentThreadId())
        return 0;
    const ContactBatch b = *input;
    Snapshot before[4];
    return preflight(b, before);
}
int contact_batch_rebuild_25661166(const ContactBatch *input, ContactBatchReport *report) {
    if (!report)
        return 0;
    *report = {};
    if (!input || !input->refresh || !input->owner_alive || !input->epoch || !input->world ||
        input->owner_thread != GetCurrentThreadId())
        return 0;
    const ContactBatch b = *input;
    Snapshot before[4], after;
    if (!preflight(b, before))
        return 0;
    report->preflight_complete = 1;
    const uint64_t sentinel = 0xc7efffffe0000000ULL;
    for (int w = 0; w < 4; w++) {
        if (!b.owner_alive(b.user, b.epoch))
            return 0;
        for (size_t i = 0; i < before[w].count; i++) {
            SIZE_T written = 0;
            report->write_attempts++;
            if (!WriteProcessMemory(GetCurrentProcess(),
                                    (void *)(before[w].nodes[i].address + 0xd0), &sentinel, 8,
                                    &written) ||
                written != 8)
                return 0;
        }
        b.refresh((void *)b.tire[w], b.dt[w], b.horizon[w], b.world, b.angular[w]);
        ++report->refresh_calls;
        if (!b.owner_alive(b.user, b.epoch) || !snapshot(b.tire[w], after) || after.bad ||
            !after.count || !after.positive)
            return 0;
        for (size_t i = 0; i < after.count; i++)
            if (after.nodes[i].aux)
                return 0;
        double delta = 0;
        for (int k = 0; k < 3; k++) {
            double d = after.position[k] - before[w].position[k];
            delta += d * d;
        }
        if (!std::isfinite(delta) || delta >= 1e-24)
            return 0;
        report->completed_mask |= 1u << w;
    }
    return b.owner_alive(b.user, b.epoch) != 0;
}
int contact_batch_flow_contacts(void *user, uint64_t epoch, int original) {
    auto *adapter = static_cast<ContactBatchFlow *>(user);
    if (!adapter || adapter->batch.epoch != epoch || (original != 0 && original != 1))
        return 0;
    return contact_batch_rebuild_25661166(&adapter->batch, &adapter->report);
}
