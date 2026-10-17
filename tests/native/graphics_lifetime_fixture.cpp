#include "../../src/native/runtime/graphics_lifetime.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "Failed: %s at %d\n", #x, __LINE__);                              \
            std::abort();                                                                          \
        }                                                                                          \
    } while (0)

struct Worker {
    GraphicsLifetime *state;
    HANDLE entered;
    HANDLE done;
};
static DWORD WINAPI publish_job(void *argument) {
    auto *w = static_cast<Worker *>(argument);
    graphics_operation_enter(w->state, true);
    SetEvent(w->entered);
    graphics_operation_leave(w->state);
    SetEvent(w->done);
    return 0;
}
static DWORD WINAPI try_from_other_thread(void *argument) {
    auto *state = static_cast<GraphicsLifetime *>(argument);
    uint64_t generation = 0;
    CHECK(!graphics_try_pin(state, &generation));
    CHECK(!graphics_pin_valid(state, 1));
    graphics_release_pin(state);
    return 0;
}
static void join(HANDLE thread) {
    CHECK(WaitForSingleObject(thread, 3000) == WAIT_OBJECT_0);
    CloseHandle(thread);
}
int main() {
    GraphicsLifetime state{};
    graphics_lifetime_init(&state);
    uint64_t generation = 0;
    CHECK(graphics_try_pin(&state, &generation) && generation == 1);
    CHECK(!graphics_try_pin(&state, &generation));
    join(CreateThread(nullptr, 0, try_from_other_thread, &state, 0, nullptr));
    CHECK(graphics_pin_valid(&state, generation));
    graphics_release_pin(&state);

    // A native loader must be able to wait for an independently published job.
    graphics_operation_enter(&state, true);
    CHECK(!graphics_try_pin(&state, &generation));
    Worker worker{&state, CreateEventW(nullptr, TRUE, FALSE, nullptr),
                  CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    HANDLE thread = CreateThread(nullptr, 0, publish_job, &worker, 0, nullptr);
    CHECK(WaitForSingleObject(worker.done, 3000) == WAIT_OBJECT_0);
    join(thread);
    graphics_operation_leave(&state);
    CHECK(graphics_try_pin(&state, &generation) && generation == 3);
    CHECK(!graphics_pin_valid(&state, 1));
    graphics_release_pin(&state);

    // New native work waits behind a reset pin, then resumes after release.
    ResetEvent(worker.entered);
    ResetEvent(worker.done);
    CHECK(graphics_try_pin(&state, &generation));
    thread = CreateThread(nullptr, 0, publish_job, &worker, 0, nullptr);
    CHECK(WaitForSingleObject(worker.entered, 50) == WAIT_TIMEOUT);
    CHECK(graphics_pin_valid(&state, generation));
    graphics_release_pin(&state);
    join(thread);
    CHECK(state.generation == 4 && state.active == 0);

    graphics_operation_enter(&state, false);
    graphics_operation_enter(&state, false);
    graphics_operation_leave(&state);
    CHECK(!graphics_try_pin(&state, &generation));
    graphics_operation_leave(&state);
    CHECK(graphics_try_pin(&state, &generation) && generation == 4);
    graphics_release_pin(&state);
    state.generation = std::numeric_limits<uint64_t>::max();
    graphics_operation_enter(&state, true);
    graphics_operation_leave(&state);
    CHECK(!graphics_try_pin(&state, &generation));
    CloseHandle(worker.entered);
    CloseHandle(worker.done);
    DeleteCriticalSection(&state.lock);
    std::puts("Graphics lifetime fixture passed: threads, async publication, pin ownership and "
              "generations.");
}
