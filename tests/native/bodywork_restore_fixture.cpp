#include "../../src/native/restore/bodywork_checkpoint.h"
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

using sector_bodywork::Backend;
using sector_bodywork::Checkpoint;
using sector_bodywork::Result;
using sector_bodywork::Transaction;

namespace {
void require(bool value, const char *expression, int line) {
    if (!value) {
        std::fprintf(stderr, "Failed at line %d: %s\n", line, expression);
        std::abort();
    }
}
#define CHECK(expression) require(bool(expression), #expression, __LINE__)
constexpr uintptr_t root = 0x100000;
constexpr uintptr_t components = root + 0x2000;
constexpr uintptr_t parts = root + 0x3000;
constexpr uintptr_t meshes = root + 0x4000;
constexpr uintptr_t submeshes = root + 0x5000;
constexpr uintptr_t vertices = root + 0x10000;
constexpr uintptr_t originals = root + 0x200000;
constexpr size_t vertices_per_mesh = 9000;
constexpr size_t vertex_bytes = vertices_per_mesh * 0x58;

struct Memory {
    std::vector<unsigned char> bytes = std::vector<unsigned char>(0x400000);
    std::vector<uintptr_t> locks;
    bool alive = true, permanent_failure = false, corrupt_write = false;
    size_t writes = 0, fault_write = 0, lock_calls = 0, busy_lock = 0;
    size_t pin_loss_write = 0;
    uintptr_t unreadable = 0;
    uint64_t epoch = 1;

    Memory() {
        set<int32_t>(root + 0x1a78, 2);
        set<uintptr_t>(root + 0x1a80, components);
        for (size_t i = 0; i < 2; ++i) {
            const auto c = components + i * 0x620;
            const auto p = parts + i * 0x28;
            const auto m = meshes + i * 0x300;
            set<int32_t>(c + 0x1c, 1);
            set<uintptr_t>(c + 0x20, p);
            set<uintptr_t>(p, m);
            set<uintptr_t>(p + 0x10, originals + i * vertex_bytes);
            set<int32_t>(m + 0x14c, vertices_per_mesh);
            set<uintptr_t>(m + 0x150, vertices + i * vertex_bytes);
            set<int32_t>(m + 0x168, 1);
            set<uintptr_t>(m + 0x170, submeshes + i * 0xe0);
            set<uint32_t>(submeshes + i * 0xe0 + 0xc8, 0xa0);
            for (size_t j = 0; j < vertex_bytes; ++j) {
                bytes[vertices - root + i * vertex_bytes + j] = static_cast<unsigned char>(j);
                bytes[originals - root + i * vertex_bytes + j] = static_cast<unsigned char>(j);
            }
        }
    }
    template <typename T> void set(uintptr_t address, T value) {
        std::memcpy(bytes.data() + address - root, &value, sizeof(value));
    }
    template <typename T> T get(uintptr_t address) const {
        T value;
        std::memcpy(&value, bytes.data() + address - root, sizeof(value));
        return value;
    }
    bool range(uintptr_t address, size_t size) const {
        return address >= root && address - root <= bytes.size() &&
               size <= bytes.size() - (address - root);
    }
    static bool pinned(void *user) {
        return static_cast<Memory *>(user)->alive;
    }
    static uint64_t generation(void *user) {
        return static_cast<Memory *>(user)->epoch;
    }
    static bool read(void *user, uintptr_t address, void *out, size_t size) {
        auto &m = *static_cast<Memory *>(user);
        if (!m.range(address, size) ||
            (m.unreadable && address <= m.unreadable && m.unreadable - address < size))
            return false;
        std::memcpy(out, m.bytes.data() + address - root, size);
        return true;
    }
    static bool write(void *user, uintptr_t address, const void *input, size_t size) {
        auto &m = *static_cast<Memory *>(user);
        CHECK(m.alive && m.locks.size() == 2);
        CHECK(m.range(address, size));
        ++m.writes;
        const bool fail = m.writes == m.fault_write ||
                          (m.permanent_failure && m.fault_write && m.writes >= m.fault_write);
        const size_t copied = fail ? size / 2 : size;
        std::memcpy(m.bytes.data() + address - root, input, copied);
        if (fail && m.corrupt_write)
            m.bytes[address - root] = static_cast<const unsigned char *>(input)[0] ^ 0xff;
        if (m.pin_loss_write == m.writes)
            m.alive = false;
        return !fail || m.corrupt_write;
    }
    static bool lock(void *user, uintptr_t address) {
        auto &m = *static_cast<Memory *>(user);
        CHECK(address == components + m.locks.size() * 0x620 + 0x5e8);
        if (++m.lock_calls == m.busy_lock)
            return false;
        m.locks.push_back(address);
        return true;
    }
    static void unlock(void *user, uintptr_t address) {
        auto &m = *static_cast<Memory *>(user);
        CHECK(!m.locks.empty() && m.locks.back() == address);
        m.locks.pop_back();
    }
    Backend backend() {
        return {this, pinned, generation, read, write, lock, unlock};
    }
    void damage() {
        for (size_t i = 0; i < 2; ++i) {
            const auto c = components + i * 0x620;
            set<uint32_t>(c + 0xc, 0x42200000);
            set<uint32_t>(c + 0x40, 2);
            set<uint32_t>(submeshes + i * 0xe0 + 0xc8, 0xb2);
            // The live capture changed byte 27. Also deform positions here so
            // the fixture does not validate only the single observed attribute.
            for (size_t j = 0; j < vertices_per_mesh; ++j) {
                bytes[vertices - root + i * vertex_bytes + j * 0x58 + 27] ^= 0xff;
                bytes[vertices - root + i * vertex_bytes + j * 0x58] ^= 0x21;
            }
        }
        set<uint32_t>(root + 0x1a88, 2);
        set<uint32_t>(root + 0x1aa4, 1);
    }
    void assert_repaired() const {
        for (size_t i = 0; i < 2; ++i) {
            CHECK(!std::memcmp(bytes.data() + vertices - root + i * vertex_bytes,
                               bytes.data() + originals - root + i * vertex_bytes, vertex_bytes));
            CHECK(get<uint32_t>(components + i * 0x620 + 0x40) == 0);
            CHECK(get<uint32_t>(components + i * 0x620 + 0xc) == 0);
            CHECK(get<unsigned char>(components + i * 0x620 + 0x18) == 1);
            // Preserve unrelated flags that changed since capture.
            CHECK(get<uint32_t>(submeshes + i * 0xe0 + 0xc8) == 0xb0);
        }
        for (size_t i = 0; i < 8; ++i)
            CHECK(get<uint32_t>(root + 0x1a88 + i * 4) == 0);
    }
};

size_t success() {
    Memory memory;
    Transaction t(memory.backend());
    Checkpoint checkpoint;
    CHECK(t.capture(root, checkpoint) == Result::ok);
    CHECK(memory.locks.empty() && memory.writes == 0);
    memory.damage();
    CHECK(t.begin(root, checkpoint) == Result::ok);
    CHECK(memory.writes == 0 && memory.locks.size() == 2);
    CHECK(t.report().meshes == 2 && t.report().vertex_bytes == 2 * vertex_bytes);
    CHECK(t.apply() == Result::ok);
    memory.assert_repaired();
    const auto count = t.report().attempted_writes;
    CHECK(count == t.report().verified_writes && count > 20);
    CHECK(t.finish(true) == Result::ok);
    CHECK(memory.locks.empty());
    return count;
}

void faults(size_t count, bool silent) {
    for (size_t fault = 1; fault <= count; ++fault) {
        Memory memory;
        Transaction t(memory.backend());
        Checkpoint checkpoint;
        CHECK(t.capture(root, checkpoint) == Result::ok);
        memory.damage();
        const auto before = memory.bytes;
        CHECK(t.begin(root, checkpoint) == Result::ok);
        memory.fault_write = fault;
        memory.corrupt_write = silent;
        CHECK(t.apply() == Result::write_failed);
        CHECK(memory.bytes == before);
        CHECK(t.report().recovery_writes == fault);
        CHECK(t.finish(false) == Result::ok);
        CHECK(memory.locks.empty());
    }
}

void recovery_and_guards() {
    Memory memory;
    Transaction t(memory.backend());
    Checkpoint checkpoint;
    CHECK(t.capture(root, checkpoint) == Result::ok);
    memory.damage();
    const auto before = memory.bytes;
    // An unsuccessful replacement must preserve the usable healthy checkpoint.
    CHECK(t.capture(root, checkpoint) == Result::damaged_checkpoint);
    CHECK(memory.writes == 0 && memory.locks.empty());
    CHECK(t.begin(root, checkpoint) == Result::ok);
    CHECK(t.apply() == Result::ok);
    CHECK(t.finish(false) == Result::ok);
    CHECK(memory.bytes == before && memory.locks.empty());

    memory.busy_lock = memory.lock_calls + 2;
    CHECK(t.begin(root, checkpoint) == Result::busy);
    CHECK(memory.locks.empty());
    memory.busy_lock = 0;
    ++memory.epoch;
    CHECK(t.begin(root, checkpoint) == Result::topology_changed);
    CHECK(memory.locks.empty());
    --memory.epoch;
    memory.set<uintptr_t>(parts + 0x10, originals + 0x58);
    CHECK(t.begin(root, checkpoint) == Result::topology_changed);
    CHECK(memory.locks.empty());
    memory.set<uintptr_t>(parts + 0x10, originals);
    memory.set<uint32_t>(meshes, 0x80);
    CHECK(t.begin(root, checkpoint) == Result::unsupported_buffer);
    memory.set<uint32_t>(meshes, 0);
    memory.set<int32_t>(meshes + 0x14c, 0x7fffffff);
    CHECK(t.begin(root, checkpoint) == Result::bad_layout);
    memory.set<int32_t>(meshes + 0x14c, vertices_per_mesh);
    memory.set<int32_t>(root + 0x1a78, -1);
    CHECK(t.begin(root, checkpoint) == Result::bad_layout);
    memory.set<int32_t>(root + 0x1a78, 2);
    memory.unreadable = vertices;
    CHECK(t.begin(root, checkpoint) == Result::read_failed);
    memory.unreadable = 0;
    memory.alive = false;
    const auto writes = memory.writes;
    CHECK(t.begin(root, checkpoint) == Result::not_pinned);
    CHECK(memory.writes == writes && memory.locks.empty());
    memory.alive = true;
    CHECK(t.begin(root, checkpoint) == Result::ok);
    // Changed bytes between begin and apply reject before the first write.
    memory.bytes[vertices - root] ^= 1;
    CHECK(t.apply() == Result::read_failed);
    CHECK(memory.writes == writes);
    CHECK(t.finish(false) == Result::ok);
}

void failed_recovery(bool lose_pin) {
    Memory memory;
    Transaction t(memory.backend());
    Checkpoint checkpoint;
    CHECK(t.capture(root, checkpoint) == Result::ok);
    memory.damage();
    CHECK(t.begin(root, checkpoint) == Result::ok);
    if (lose_pin) {
        memory.pin_loss_write = 2;
    } else {
        memory.fault_write = 2;
        memory.permanent_failure = true;
    }
    CHECK(t.apply() == Result::recovery_failed);
    if (lose_pin)
        CHECK(memory.writes == 2); // No speculative writes after losing ownership.
    CHECK(t.finish(false) == Result::recovery_failed);
    CHECK(memory.locks.empty());
    CHECK(t.begin(root, checkpoint) == Result::wrong_phase);
}

void replay_capture(const std::filesystem::path &folder) {
    Memory memory;
    memory.bytes.assign(12 * 1024 * 1024, 0);
    memory.set<int32_t>(root + 0x1a78, 1);
    memory.set<uintptr_t>(root + 0x1a80, components);
    memory.set<int32_t>(components + 0x1c, 4);
    memory.set<uintptr_t>(components + 0x20, parts);
    std::vector<std::vector<unsigned char>> damaged;
    std::vector<uintptr_t> destinations;
    size_t offset = 0, differences = 0;
    for (size_t i = 0; i < 4; ++i) {
        const auto prefix = "mesh-0-" + std::to_string(i);
        auto read_file = [&](const char *suffix) {
            std::ifstream file(folder / (prefix + suffix), std::ios::binary);
            CHECK(file.good());
            return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), {});
        };
        auto original = read_file("-original.bin");
        auto deformed = read_file("-deformed.bin");
        CHECK(!original.empty() && original.size() == deformed.size());
        CHECK(original.size() % 0x58 == 0 && original.size() <= 4 * 1024 * 1024 - offset);
        const auto m = meshes + i * 0x300;
        const auto destination = vertices + offset;
        const auto source = root + 0x600000 + offset;
        memory.set<uintptr_t>(parts + i * 0x28, m);
        memory.set<uintptr_t>(parts + i * 0x28 + 0x10, source);
        memory.set<int32_t>(m + 0x14c, int32_t(original.size() / 0x58));
        memory.set<uintptr_t>(m + 0x150, destination);
        std::memcpy(memory.bytes.data() + destination - root, original.data(), original.size());
        std::memcpy(memory.bytes.data() + source - root, original.data(), original.size());
        for (size_t j = 0; j < original.size(); ++j)
            differences += original[j] != deformed[j];
        offset += original.size();
        destinations.push_back(destination);
        damaged.push_back(std::move(deformed));
    }
    // Replay uses synthetic metadata around recorded vertex bytes, not pointers
    // from another process. The backend still owns every byte that can be written.
    auto backend = memory.backend();
    backend.write = [](void *user, uintptr_t address, const void *input, size_t size) {
        auto &m = *static_cast<Memory *>(user);
        CHECK(m.alive && m.locks.size() == 1 && m.range(address, size));
        std::memcpy(m.bytes.data() + address - root, input, size);
        ++m.writes;
        return true;
    };
    Transaction transaction(backend);
    Checkpoint checkpoint;
    CHECK(transaction.capture(root, checkpoint) == Result::ok);
    for (size_t i = 0; i < damaged.size(); ++i)
        std::memcpy(memory.bytes.data() + destinations[i] - root, damaged[i].data(),
                    damaged[i].size());
    memory.set<uint32_t>(components + 0x40, 2);
    CHECK(transaction.begin(root, checkpoint) == Result::ok);
    CHECK(transaction.apply() == Result::ok);
    CHECK(std::memcmp(memory.bytes.data() + vertices - root, memory.bytes.data() + 0x600000,
                      offset) == 0);
    CHECK(transaction.finish(true) == Result::ok && memory.locks.empty());
    std::printf("Captured vertices replay passed: %zu bytes, %zu differing bytes.\n", offset,
                differences);
}
} // namespace

int main(int argc, char **argv) {
    const auto count = success();
    faults(count, false);
    faults(count, true);
    recovery_and_guards();
    failed_recovery(false);
    failed_recovery(true);
    if (argc == 2)
        replay_capture(argv[1]);
    std::printf("Bodywork fixture passed: %zu spans, %zu write-fault cases; no game access.\n",
                count, count * 2);
}
