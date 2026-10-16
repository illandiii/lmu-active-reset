#ifndef LMU_PHASE_IPC_H
#define LMU_PHASE_IPC_H
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#define PHASE_MAGIC 0x4c4d555048415331ULL
struct PhaseFrame {
    uint64_t tick_ms, dynamics_thread, scoring_thread, scoring_calls, overlaps, thread_changes,
        owner, physics, scoring, score_sequence, stable, current, remembered, last_computed;
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
static_assert(sizeof(PhaseFrame) == 112 && sizeof(PhaseIPC) == 208, "Phase ABI");
static void phase_mapping_name(wchar_t *out, DWORD pid) {
    swprintf(out, 96, L"Local\\LMU_Phase25661166_v1_%lu", pid);
}
#endif
