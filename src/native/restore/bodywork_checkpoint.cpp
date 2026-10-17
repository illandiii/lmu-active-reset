#include "bodywork_checkpoint.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace sector_bodywork {
namespace {
using Bytes = std::vector<unsigned char>;
constexpr size_t component_stride = 0x620;
constexpr size_t part_stride = 0x28;
constexpr size_t vertex_stride = 0x58;
constexpr size_t submesh_stride = 0xe0;
constexpr size_t max_components = 32;
constexpr size_t max_meshes = 256;
constexpr size_t max_submeshes = 4096;
constexpr size_t max_vertex_bytes = 32 * 1024 * 1024;
constexpr size_t chunk_bytes = 64 * 1024;

bool address_range(uintptr_t address, size_t size) {
    return address >= 0x10000 && size && address < 0x800000000000ULL &&
           size <= 0x800000000000ULL - address;
}

struct Mesh {
    uintptr_t record = 0, object = 0, original = 0, vertices = 0, submeshes = 0;
    int32_t count = 0, submesh_count = 0;
    Bytes bytes;
    std::vector<uint32_t> flags;
};
struct Component {
    uintptr_t address = 0, parts = 0;
    uint32_t time_bits = 0, events = 0;
    unsigned char dirty = 0;
    std::vector<Mesh> meshes;
};
struct Image {
    uintptr_t root = 0, entries = 0;
    uint64_t generation = 0;
    std::array<uint32_t, 8> damage{};
    std::vector<Component> components;
};
struct Patch {
    uintptr_t address = 0;
    Bytes before, after;
};

bool same_topology(const Image &a, const Image &b) {
    if (a.root != b.root || a.entries != b.entries || a.generation != b.generation ||
        a.components.size() != b.components.size())
        return false;
    for (size_t i = 0; i < a.components.size(); ++i) {
        const auto &x = a.components[i];
        const auto &y = b.components[i];
        if (x.address != y.address || x.parts != y.parts || x.meshes.size() != y.meshes.size())
            return false;
        for (size_t j = 0; j < x.meshes.size(); ++j) {
            const auto &m = x.meshes[j];
            const auto &n = y.meshes[j];
            if (m.record != n.record || m.object != n.object || m.original != n.original ||
                m.vertices != n.vertices || m.submeshes != n.submeshes || m.count != n.count ||
                m.submesh_count != n.submesh_count)
                return false;
        }
    }
    return true;
}
} // namespace

struct Checkpoint::Data {
    Image image;
};
Checkpoint::Checkpoint() = default;
Checkpoint::~Checkpoint() = default;
Checkpoint::Checkpoint(Checkpoint &&) noexcept = default;
Checkpoint &Checkpoint::operator=(Checkpoint &&) noexcept = default;

struct Transaction::Work {
    Backend backend;
    Report report;
    std::vector<uintptr_t> locks;
    std::vector<Patch> patches;
    size_t touched = 0;
    uint64_t generation = 0;
    bool begun = false, applied = false, poisoned = false;

    explicit Work(Backend b) : backend(b) {}
    Result result(Result r) {
        return report.result = r;
    }
    bool valid_backend() const {
        return backend.pinned && backend.generation && backend.read && backend.write &&
               backend.try_lock && backend.unlock;
    }
    bool pinned() const {
        return valid_backend() && generation && backend.pinned(backend.user) &&
               backend.generation(backend.user) == generation;
    }
    bool read(uintptr_t address, void *out, size_t size) const {
        return address_range(address, size) && pinned() &&
               backend.read(backend.user, address, out, size) && pinned();
    }
    template <typename T> bool read(uintptr_t address, T &out) const {
        return read(address, &out, sizeof(out));
    }
    void unlock() {
        for (auto it = locks.rbegin(); it != locks.rend(); ++it)
            backend.unlock(backend.user, *it);
        locks.clear();
    }
    Result load(uintptr_t root, Image &image, bool healthy) {
        int32_t count = 0;
        if (!address_range(root, 0x1aa8))
            return Result::bad_layout;
        if (!read(root + 0x1a78, count) || !read(root + 0x1a80, image.entries))
            return Result::read_failed;
        if (count <= 0 || count > int32_t(max_components) ||
            !address_range(image.entries, size_t(count) * component_stride))
            return Result::bad_layout;
        image.root = root;
        image.generation = generation;
        locks.reserve(size_t(count));
        // All locks must be held before inspecting buffers or changing anything.
        // Never wait while a simulation/control/lifetime pin is held.
        for (int32_t i = 0; i < count; ++i) {
            const auto lock = image.entries + size_t(i) * component_stride + 0x5e8;
            if (!pinned())
                return Result::not_pinned;
            if (!backend.try_lock(backend.user, lock))
                return Result::busy;
            locks.push_back(lock);
        }
        int32_t count_after = 0;
        uintptr_t entries_after = 0;
        if (!read(root + 0x1a78, count_after) || !read(root + 0x1a80, entries_after))
            return Result::read_failed;
        if (count_after != count || entries_after != image.entries)
            return Result::topology_changed;
        if (!read(root + 0x1a88, image.damage))
            return Result::read_failed;
        for (auto damage : image.damage)
            if (damage > 2 || (healthy && damage))
                return healthy ? Result::damaged_checkpoint : Result::bad_layout;
        image.components.resize(size_t(count));
        for (size_t i = 0; i < image.components.size(); ++i) {
            auto &component = image.components[i];
            component.address = image.entries + i * component_stride;
            unsigned char replay = 0;
            int32_t parts = 0;
            if (!read(component.address + 8, replay) ||
                !read(component.address + 0xc, component.time_bits) ||
                !read(component.address + 0x18, component.dirty) ||
                !read(component.address + 0x1c, parts) ||
                !read(component.address + 0x20, component.parts) ||
                !read(component.address + 0x40, component.events))
                return Result::read_failed;
            if (replay || component.dirty > 1 || component.events > 20 || parts <= 0 ||
                size_t(parts) > max_meshes - report.meshes ||
                !address_range(component.parts, size_t(parts) * part_stride))
                return Result::bad_layout;
            if (healthy && component.events)
                return Result::damaged_checkpoint;
            component.meshes.resize(size_t(parts));
            for (size_t j = 0; j < component.meshes.size(); ++j) {
                auto &mesh = component.meshes[j];
                mesh.record = component.parts + j * part_stride;
                uint32_t flags = 0;
                if (!read(mesh.record, mesh.object) || !read(mesh.record + 0x10, mesh.original))
                    return Result::read_failed;
                if (!address_range(mesh.object, 0x178))
                    return Result::bad_layout;
                if (!read(mesh.object, flags) || !read(mesh.object + 0x14c, mesh.count) ||
                    !read(mesh.object + 0x150, mesh.vertices) ||
                    !read(mesh.object + 0x168, mesh.submesh_count) ||
                    !read(mesh.object + 0x170, mesh.submeshes))
                    return Result::read_failed;
                // Native 52C5B0 can map a GPU buffer for this flag. That path needs
                // its own thread contract; do not treat its CPU address as writable.
                if (flags & 0x80)
                    return Result::unsupported_buffer;
                if (mesh.count <= 0 ||
                    size_t(mesh.count) > (max_vertex_bytes - report.vertex_bytes) / vertex_stride ||
                    mesh.submesh_count < 0 || size_t(mesh.submesh_count) > max_submeshes)
                    return Result::bad_layout;
                const size_t bytes = size_t(mesh.count) * vertex_stride;
                if (!address_range(mesh.vertices, bytes) || !address_range(mesh.original, bytes) ||
                    (mesh.submesh_count &&
                     !address_range(mesh.submeshes, size_t(mesh.submesh_count) * submesh_stride)))
                    return Result::bad_layout;
                mesh.bytes.resize(bytes);
                if (!read(mesh.vertices, mesh.bytes.data(), bytes))
                    return Result::read_failed;
                if (healthy) {
                    Bytes original(bytes);
                    if (!read(mesh.original, original.data(), bytes))
                        return Result::read_failed;
                    if (mesh.bytes != original)
                        return Result::damaged_checkpoint;
                }
                mesh.flags.resize(size_t(mesh.submesh_count));
                for (size_t k = 0; k < mesh.flags.size(); ++k) {
                    if (!read(mesh.submeshes + k * submesh_stride + 0xc8, mesh.flags[k]))
                        return Result::read_failed;
                    if (healthy && (mesh.flags[k] & 2))
                        return Result::damaged_checkpoint;
                }
                report.vertex_bytes += bytes;
                ++report.meshes;
            }
            ++report.components;
        }
        return pinned() ? Result::ok : Result::not_pinned;
    }
    void patch(uintptr_t address, const void *before, const void *after, size_t size) {
        const auto *a = static_cast<const unsigned char *>(before);
        const auto *b = static_cast<const unsigned char *>(after);
        for (size_t offset = 0; offset < size; offset += chunk_bytes) {
            const auto n = std::min(chunk_bytes, size - offset);
            if (std::memcmp(a + offset, b + offset, n) != 0)
                patches.push_back({address + offset, Bytes(a + offset, a + offset + n),
                                   Bytes(b + offset, b + offset + n)});
        }
    }
    bool verify(const Patch &p, const Bytes &expected) const {
        std::array<unsigned char, chunk_bytes> bytes;
        return read(p.address, bytes.data(), expected.size()) &&
               std::memcmp(bytes.data(), expected.data(), expected.size()) == 0;
    }
    bool matches(uintptr_t address, const Bytes &expected) const {
        std::array<unsigned char, chunk_bytes> bytes;
        for (size_t offset = 0; offset < expected.size(); offset += chunk_bytes) {
            const auto size = std::min(chunk_bytes, expected.size() - offset);
            if (!read(address + offset, bytes.data(), size) ||
                std::memcmp(bytes.data(), expected.data() + offset, size) != 0)
                return false;
        }
        return true;
    }
    bool write(const Patch &p, const Bytes &value) const {
        return pinned() && backend.write(backend.user, p.address, value.data(), value.size()) &&
               verify(p, value);
    }
};

Transaction::Transaction(Backend backend) : work_(std::make_unique<Work>(backend)) {}
Transaction::~Transaction() {
    if (work_->begun)
        finish(false);
    else
        work_->unlock();
}
const Report &Transaction::report() const {
    return work_->report;
}

Result Transaction::capture(uintptr_t graphics, Checkpoint &checkpoint) {
    auto &w = *work_;
    if (w.begun || w.poisoned)
        return w.result(Result::wrong_phase);
    w.report = {};
    if (!w.valid_backend())
        return w.result(Result::invalid_backend);
    w.generation = w.backend.generation(w.backend.user);
    if (!w.pinned())
        return w.result(Result::not_pinned);
    Result r;
    try {
        auto next = std::make_unique<Checkpoint::Data>();
        r = w.load(graphics, next->image, true);
        if (r == Result::ok)
            checkpoint.data_ = std::move(next);
    } catch (const std::bad_alloc &) {
        r = Result::allocation_failed;
    }
    w.unlock();
    return w.result(r);
}

Result Transaction::begin(uintptr_t graphics, const Checkpoint &checkpoint) {
    auto &w = *work_;
    if (w.begun || w.poisoned || !checkpoint.data_)
        return w.result(Result::wrong_phase);
    w.report = {};
    w.patches.clear();
    if (!w.valid_backend())
        return w.result(Result::invalid_backend);
    w.generation = w.backend.generation(w.backend.user);
    if (!w.pinned())
        return w.result(Result::not_pinned);
    if (checkpoint.data_->image.generation != w.generation)
        return w.result(Result::topology_changed);
    Result r = Result::ok;
    try {
        Image current;
        r = w.load(graphics, current, false);
        const auto &saved = checkpoint.data_->image;
        if (r == Result::ok && !same_topology(current, saved))
            r = Result::topology_changed;
        // A reused source allocation must not silently substitute a different
        // clean model at the old addresses, even if counts still match.
        if (r == Result::ok)
            for (const auto &component : saved.components)
                for (const auto &mesh : component.meshes)
                    if (!w.matches(mesh.original, mesh.bytes))
                        r = Result::topology_changed;
        if (r == Result::ok) {
            for (size_t i = 0; i < current.components.size(); ++i) {
                const auto &c = current.components[i];
                for (size_t j = 0; j < c.meshes.size(); ++j) {
                    const auto &mesh = c.meshes[j];
                    const auto &desired = saved.components[i].meshes[j];
                    w.patch(mesh.vertices, mesh.bytes.data(), desired.bytes.data(),
                            mesh.bytes.size());
                    for (size_t k = 0; k < mesh.flags.size(); ++k) {
                        const uint32_t flags = mesh.flags[k] & ~uint32_t(2);
                        w.patch(mesh.submeshes + k * submesh_stride + 0xc8, &mesh.flags[k], &flags,
                                sizeof(flags));
                    }
                }
                const uint32_t zero = 0;
                const unsigned char dirty = 1;
                w.patch(c.address + 0xc, &c.time_bits, &zero, sizeof(zero));
                w.patch(c.address + 0x40, &c.events, &zero, sizeof(zero));
                w.patch(c.address + 0x18, &c.dirty, &dirty, sizeof(dirty));
            }
            const std::array<uint32_t, 8> clean{};
            w.patch(graphics + 0x1a88, current.damage.data(), clean.data(), sizeof(clean));
            // Aliased vertex buffers or metadata would make a rollback ambiguous.
            std::vector<std::pair<uintptr_t, uintptr_t>> ranges;
            for (const auto &p : w.patches)
                ranges.emplace_back(p.address, p.address + p.before.size());
            std::sort(ranges.begin(), ranges.end());
            for (size_t i = 1; i < ranges.size(); ++i)
                if (ranges[i].first < ranges[i - 1].second)
                    r = Result::bad_layout;
        }
    } catch (const std::bad_alloc &) {
        r = Result::allocation_failed;
    }
    if (r != Result::ok) {
        w.unlock();
        w.patches.clear();
    } else {
        w.begun = true;
        w.applied = false;
        w.touched = 0;
    }
    return w.result(r);
}

Result Transaction::apply() {
    auto &w = *work_;
    if (!w.begun || w.applied || w.touched || w.poisoned)
        return w.result(Result::wrong_phase);
    // Preflight every span before the first write. A later partial write must be
    // included in recovery even when the backend reports that write as failed.
    for (const auto &p : w.patches)
        if (!w.verify(p, p.before))
            return w.result(w.pinned() ? Result::read_failed : Result::not_pinned);
    for (const auto &p : w.patches) {
        ++w.touched;
        ++w.report.attempted_writes;
        if (!w.write(p, p.after)) {
            const auto recovered = rollback();
            return w.result(recovered == Result::ok ? Result::write_failed : recovered);
        }
        ++w.report.verified_writes;
    }
    w.applied = true;
    return w.result(Result::ok);
}

Result Transaction::rollback() {
    auto &w = *work_;
    if (!w.begun || w.poisoned)
        return w.result(Result::wrong_phase);
    bool recovered = true;
    for (size_t i = w.touched; i > 0; --i) {
        ++w.report.recovery_writes;
        const auto &p = w.patches[i - 1];
        if (!w.write(p, p.before))
            recovered = false;
    }
    if (!recovered) {
        w.poisoned = true;
        return w.result(Result::recovery_failed);
    }
    w.touched = 0;
    w.applied = false;
    return w.result(Result::ok);
}

Result Transaction::finish(bool commit) {
    auto &w = *work_;
    if (!w.begun)
        return w.result(Result::wrong_phase);
    Result r = w.poisoned ? Result::recovery_failed : Result::ok;
    if (commit && (!w.applied || !w.pinned())) {
        commit = false;
        r = Result::wrong_phase;
    }
    if (!commit && w.touched && !w.poisoned) {
        const auto recovered = rollback();
        if (recovered != Result::ok)
            r = recovered;
    }
    w.unlock();
    w.begun = false;
    w.applied = false;
    w.touched = 0;
    w.patches.clear();
    return w.result(r);
}
} // namespace sector_bodywork
