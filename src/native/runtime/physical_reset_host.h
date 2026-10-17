#ifndef LMU_PHYSICAL_RESET_HOST_25661166_H
#define LMU_PHYSICAL_RESET_HOST_25661166_H
#include <windows.h>
#include <stdint.h>
#ifndef LMU_RESET_HOST_REVISION
#define LMU_RESET_HOST_REVISION 1
#endif
constexpr uint64_t RESET_HOST_MAGIC = 0x4c4d555253543030ULL + LMU_RESET_HOST_REVISION;
enum HostAction { HOST_SAVE = 1, HOST_PREPARE = 2, HOST_RESTORE = 3 };
enum HostStopReason { HOST_STOP_REQUESTED = 0, HOST_STOP_LIFECYCLE = 1 };
struct ResetHostReport {
    uint64_t action, result, fields, forward_writes, recovery_writes, flow_result, cause,
        contact_mask, contact_calls, saves, loads, callbacks, pins, epoch, readable,
        verified_fields, mismatched_fields;
    double saved_position[3], before_position[3], after_position[3], speed;
};
struct ResetHostIPC {
    uint64_t magic, version, pid;
    volatile LONG stop, status;
    volatile LONG64 heartbeat_ms;
    volatile LONG allow, permit_writes;
    uint64_t owner_thread;
    volatile LONG64 request_id, request_action, request_tick;
    volatile LONG64 report_sequence, completed_id;
    volatile LONG error, reserved;
    ResetHostReport report;
    volatile LONG64 physical_field_write_attempts;
#if LMU_RESET_HOST_REVISION >= 21
    uint64_t bodywork_result, bodywork_components, bodywork_meshes, bodywork_vertex_bytes,
        bodywork_attempted_writes, bodywork_verified_writes, bodywork_recovery_writes;
#endif
};
static_assert(sizeof(ResetHostReport) == 216, "Reset report ABI");
#if LMU_RESET_HOST_REVISION >= 21
static_assert(sizeof(ResetHostIPC) == 384, "Reset host ABI with bodywork diagnostics");
#else
static_assert(sizeof(ResetHostIPC) == 328, "Legacy reset host ABI");
#endif
#endif
