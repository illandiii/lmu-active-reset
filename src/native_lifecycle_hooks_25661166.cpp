#include "native_lifecycle_hooks_25661166.h"
#include "MinHook.h"
#include <cstring>

static NativeLifecycleEpoch *guard;
static void(WINAPI *clear_original)(void *, unsigned char);
static void(WINAPI *reconstruct_original)(void *);
static void(WINAPI *destroy_original)(void *);
static void *(WINAPI *construct_original)(void *);
static void(WINAPI *vehicle_destroy_original)(void *);
static void *(WINAPI *vehicle_construct_original)(void *);
static void(WINAPI *control_original)(void *, int, char, char);

struct Mutation {
    Mutation() {
        native_lifecycle_mutation_enter(guard);
    }
    ~Mutation() {
        native_lifecycle_mutation_leave(guard);
    }
};
static void WINAPI clear(void *object, unsigned char release) {
    Mutation scope;
    clear_original(object, release);
}
static void WINAPI reconstruct(void *object) {
    Mutation scope;
    reconstruct_original(object);
}
static void WINAPI destroy(void *object) {
    Mutation scope;
    destroy_original(object);
}
static void *WINAPI construct(void *object) {
    Mutation scope;
    return construct_original(object);
}
static void WINAPI vehicle_destroy(void *object) {
    Mutation scope;
    vehicle_destroy_original(object);
}
static void *WINAPI vehicle_construct(void *object) {
    Mutation scope;
    return vehicle_construct_original(object);
}
static void WINAPI control(void *object, int mode, char remote, char replay) {
    Mutation scope;
    control_original(object, mode, remote, replay);
}

bool native_lifecycle_hooks_create(uintptr_t base, NativeLifecycleEpoch *epoch) {
    if (!base || !epoch || guard)
        return false;
    struct Target {
        uintptr_t rva;
        const char *fixture;
        void *hook;
        void **original;
    };
    const Target entries[]{
        {0xed0b50, "FixtureClear", reinterpret_cast<void *>(clear),
         reinterpret_cast<void **>(&clear_original)},
        {0xee32d0, "FixtureReconstruct", reinterpret_cast<void *>(reconstruct),
         reinterpret_cast<void **>(&reconstruct_original)},
        {0xee3320, "FixtureDestroy", reinterpret_cast<void *>(destroy),
         reinterpret_cast<void **>(&destroy_original)},
        {0xee35d0, "FixtureConstruct", reinterpret_cast<void *>(construct),
         reinterpret_cast<void **>(&construct_original)},
        {0xf720e0, "FixtureVehicleDestroy", reinterpret_cast<void *>(vehicle_destroy),
         reinterpret_cast<void **>(&vehicle_destroy_original)},
        {0xf722c0, "FixtureVehicleConstruct", reinterpret_cast<void *>(vehicle_construct),
         reinterpret_cast<void **>(&vehicle_construct_original)},
        {0xf73de0, "FixtureControl", reinterpret_cast<void *>(control),
         reinterpret_cast<void **>(&control_original)},
    };
    // Expected prologs are checked as part of the host's complete target manifest
    // before any hook is enabled. Fixtures resolve their own exported functions.
    guard = epoch;
    for (const auto &entry : entries) {
#ifdef LMU_LIFECYCLE_FIXTURE
        auto *target = reinterpret_cast<void *>(
            GetProcAddress(reinterpret_cast<HMODULE>(base), entry.fixture));
#else
        auto *target = reinterpret_cast<void *>(base + entry.rva);
#endif
        if (!target || MH_CreateHook(target, entry.hook, entry.original) != MH_OK)
            return false;
    }
    return true;
}
