#pragma once
#include <windows.h>
#include <stdint.h>
struct ResetTraceRecord {
    uint64_t phase, request, tick, step, valid;
    double bodies[5][18];  // position, velocity, orientation, angular momentum
    double contacts[4][6]; // node count, min/max depth, normal of deepest node
#if LMU_RESET_HOST_REVISION >= 12
    uint64_t collision_flags;
    double collision_body[18];
    float exported_vehicle[24]; // native contiguous vehicle values at 159E8
    double hdv_pose[15];        // aggregate position, orientation, reference position
    double collision_feedback[6];
#endif
};
#if LMU_RESET_HOST_REVISION >= 12
static_assert(sizeof(ResetTraceRecord) == 1368, "Physical trace ABI");
#else
static_assert(sizeof(ResetTraceRecord) == 952, "Physical trace ABI");
#endif
struct ResetTrace {
    volatile LONG64 count;
    ResetTraceRecord records[512];
};
