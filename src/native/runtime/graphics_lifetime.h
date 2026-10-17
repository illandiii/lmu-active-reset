#ifndef SECTOR_GRAPHICS_LIFETIME_H
#define SECTOR_GRAPHICS_LIFETIME_H
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>

// Native graphics operations can wait for jobs running on another thread. Never
// retain this mutex across a native operation: register activity, then release
// it. A reset pins only while there are no registered operations in progress.
struct GraphicsLifetime {
    CRITICAL_SECTION lock;
    uint64_t generation = 1;
    uint64_t active = 0;
    DWORD pin_thread = 0;
};
void graphics_lifetime_init(GraphicsLifetime *);
void graphics_operation_enter(GraphicsLifetime *, bool reconstruct);
void graphics_operation_leave(GraphicsLifetime *);
bool graphics_try_pin(GraphicsLifetime *, uint64_t *generation);
bool graphics_pin_valid(GraphicsLifetime *, uint64_t generation);
void graphics_release_pin(GraphicsLifetime *);

// Install after normal compatibility checks and before enabling MinHook.
// Observes only the nominated player's graphics root and its components.
bool graphics_lifetime_hooks_create(uintptr_t module, uintptr_t graphics, GraphicsLifetime *);
#endif
