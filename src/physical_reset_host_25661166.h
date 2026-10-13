#ifndef LMU_PHYSICAL_RESET_HOST_25661166_H
#define LMU_PHYSICAL_RESET_HOST_25661166_H
#include <windows.h>
#include <stdint.h>
#ifndef LMU_RESET_HOST_REVISION
#define LMU_RESET_HOST_REVISION 1
#endif
constexpr uint64_t RESET_HOST_MAGIC = 0x4c4d555253543030ULL + LMU_RESET_HOST_REVISION;
enum HostAction { HOST_SAVE = 1, HOST_PREPARE = 2, HOST_RESTORE = 3 };
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
};
static_assert(sizeof(ResetHostReport) == 216 && sizeof(ResetHostIPC) == 328, "Reset host ABI");
#endif
