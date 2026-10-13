#ifndef LMU_BOUNDARY_IPC_H
#define LMU_BOUNDARY_IPC_H
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#define PHASE_MAGIC 0x4c4d555048415334ULL
struct PhaseFrame {
    uint64_t tick_ms, dynamics_thread, scoring_thread, scoring_calls, overlaps, thread_changes,
        owner, physics, scoring, score_sequence, stable, current, remembered, last_computed,
        boundary_thread, boundary_calls, boundary_depth, outside_boundary, lock_owner,
        lock_recursion, boundary_lock_held;
    uint64_t control_owner_before, pin_acquired, pin_valid, pin_released;
};
struct PhaseIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t qpc_frequency;
    PhaseFrame frame;
};
static_assert(sizeof(PhaseFrame) == 200 && sizeof(PhaseIPC) == 296, "Phase ABI");
static void phase_mapping_name(wchar_t *out, DWORD pid) {
    swprintf(out, 96, L"Local\\LMU_Boundary25661166_v3_%lu", pid);
}
#endif
