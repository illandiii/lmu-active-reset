#pragma once
#include <windows.h>
#include <stdint.h>
struct ResetTraceRecord {
    uint64_t phase, request, tick, step, valid;
    double bodies[5][18];  // position, velocity, orientation, angular momentum
    double contacts[4][6]; // node count, min/max depth, normal of deepest node
};
struct ResetTrace {
    volatile LONG64 count;
    ResetTraceRecord records[512];
};
