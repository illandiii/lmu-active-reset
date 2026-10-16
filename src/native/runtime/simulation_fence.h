#ifndef LMU_SIMULATION_FENCE_H
#define LMU_SIMULATION_FENCE_H
#include <windows.h>
#include <stdint.h>
/* Fences the registered player callbacks, not unrelated game threads or memory
   allocators. The boundary must enclose every relevant parent continuation;
   fencing a child Dynamics call does not stop writes in its caller. Host must
   establish object lifetime before enabling restoration. */
enum FencePhase { FENCE_BOUNDARY, FENCE_SCORING, FENCE_CONTACTS, FENCE_PHASES };
enum FenceMode { FENCE_RUNNING, FENCE_HELD, FENCE_QUARANTINED, FENCE_STOPPING };
struct SimulationFence {
    SRWLOCK lock;
    uintptr_t object[FENCE_PHASES];
    uint64_t epoch;
    DWORD owner_thread;
    FenceMode mode;
    unsigned active[FENCE_PHASES];
    uint64_t blocked[FENCE_PHASES];
    bool violated, dirty;
    uintptr_t contact_objects[4];
    unsigned contact_count;
};
struct FenceTicket {
    SimulationFence *fence;
    unsigned phase;
    bool admitted;
};
void simulation_fence_init(SimulationFence *, uint64_t, DWORD, const uintptr_t *,
                           const uintptr_t *contacts = nullptr, unsigned contact_count = 0);
/* Foreign objects bypass the fence (admitted, no ticket). Matching objects are
   counted until leave; callers MUST NOT invoke originals after rejection. */
bool simulation_fence_callback_enter(SimulationFence *, unsigned, uintptr_t, FenceTicket *);
void simulation_fence_callback_leave(FenceTicket *);
/* Recheck after a restore before forwarding the CURRENT outer update call. */
bool simulation_fence_may_advance(const FenceTicket *);
bool simulation_fence_hold(SimulationFence *, uint64_t);
bool simulation_fence_owner_alive(SimulationFence *, uint64_t);
void simulation_fence_mark_dirty(SimulationFence *);
void simulation_fence_release(SimulationFence *, bool verified_resume);
void simulation_fence_quarantine(SimulationFence *);
/* Stops admitting tracked callbacks; false means hooks must remain installed.
   Cannot release a held/quarantined vehicle merely because heartbeat expired. */
bool simulation_fence_stop(SimulationFence *);
#endif
