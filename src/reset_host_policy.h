#ifndef LMU_RESET_HOST_POLICY_H
#define LMU_RESET_HOST_POLICY_H
#include <stdint.h>

// Controller liveness, not session age. A healthy controller may run indefinitely.
static inline bool reset_host_controller_alive(uint64_t now, uint64_t heartbeat) {
    return heartbeat <= now && now - heartbeat <= 2000;
}
#endif
