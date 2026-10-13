#ifndef LMU_CONTACT_IPC_H
#define LMU_CONTACT_IPC_H
#include <windows.h>
#include <stdint.h>
#define CONTACT_MAGIC 0x4c4d55434f4e5432ULL
#define CONTACT_MAX 512
typedef struct ContactResult {
    uint64_t wheel, tick_ms, samples, pre_count, post_count, pre_bad, post_bad;
    uint64_t removed, added, post_positive, surface_count, retired, cold_flags_clear,
        native_stationary;
    double dt, horizon, angular_speed, duration_us, pre_min_depth, post_min_depth;
    double post_max_depth, normal_min, normal_max, position_delta;
} ContactResult;
typedef struct ContactIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow_observation, error;
    volatile LONG64 callbacks, observations, skipped, sequence, writer_thread;
    volatile LONG64 request_id, request_deadline_ms;
    volatile LONG allow_rebuild, completed_mask, failed_mask, reserved;
    uint64_t qpc_frequency, physics_writes, request_seen;
    ContactResult latest[4], rebuilt[4];
} ContactIPC;
static void contact_mapping_name(wchar_t *out, DWORD pid) {
    wsprintfW(out, L"Local\\LMU_Contact25661166_v2_%lu", pid);
}
#endif
