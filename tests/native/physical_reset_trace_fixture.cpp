#include "../../src/native/runtime/physical_reset_trace.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <cstddef>

int main() {
    static_assert(sizeof(ResetTraceRecord) == 1368, "v14 payload ABI");
    static_assert(offsetof(ResetTrace, published) == 700424, "v14 stamp offset");
    static_assert(sizeof(ResetTrace) == 704520, "v14 mapping ABI");
    auto trace = std::make_unique<ResetTrace>();
    constexpr unsigned total = 512 * 3 + 19;
    for (unsigned i = 0; i < total; ++i) {
        ResetTraceRecord row{};
        row.phase = 11;
        row.request = i + 1;
        row.collision_body[17] = -(static_cast<double>(i) + 1);
        reset_trace_append(trace.get(), row);
        assert(trace->count == i + 1);
        assert(trace->published[i % 512] == i + 1);
    }
    for (unsigned i = total - 512; i < total; ++i) {
        assert(trace->records[i % 512].request == i + 1);
        assert(trace->records[i % 512].collision_body[17] == -(static_cast<double>(i) + 1));
        assert(trace->published[i % 512] == i + 1);
    }
    puts("PASS physical trace: 1555 publications, bounded ring reuse and exact v14 layout.");
}
