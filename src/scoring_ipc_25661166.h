#ifndef LMU_SCORING_IPC_H
#define LMU_SCORING_IPC_H
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include "lap_guard_core.h"
#define SCORING_MAGIC 0x4c4d5553434f5231ULL
struct ScoringFrame {
    uint64_t tick_ms;
    int32_t lap_before, lap_after;
    LapGuardFlags before, after, proposed;
    float dt;
    uint32_t prepared;
    double observer_us;
};
struct ScoringIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    uint64_t qpc_frequency;
    ScoringFrame frame;
};
static_assert(sizeof(ScoringFrame) == 80 && sizeof(ScoringIPC) == 176, "Scoring ABI");
static void scoring_mapping_name(wchar_t *out, DWORD pid) {
    swprintf(out, 96, L"Local\\LMU_Scoring25661166_v1_%lu", pid);
}
#endif
