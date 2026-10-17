#include "graphics_lifetime.h"
#include "MinHook.h"

static GraphicsLifetime *lifetime;
static uintptr_t player_graphics;
static void(WINAPI *clear_original)(void *);
static void(WINAPI *load_original)(void *);
static void *(WINAPI *construct_original)(void *);
static void(WINAPI *destroy_original)(void *);
static void(WINAPI *publish_original)(void *, void *);
static void(WINAPI *component_clear_original)(void *);
static void(WINAPI *component_load_original)(void *, void *, void *, void *);
static void(WINAPI *impact_original)(void *, float, void *, void *);
static void(WINAPI *repair_original)(void *);
static void(WINAPI *reset_original)(void *, unsigned char);

template <typename T> static bool field(uintptr_t address, T &out) {
    SIZE_T bytes = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(address), &out,
                             sizeof(out), &bytes) &&
           bytes == sizeof(out);
}
static bool player_component(void *object) {
    // Root mutations are already registered before replacing this allocation.
    // An asynchronous publisher that outlives load is registered independently.
    uintptr_t entries = 0;
    int32_t count = 0;
    const auto address = reinterpret_cast<uintptr_t>(object);
    return field(player_graphics + 0x1a78, count) && count > 0 && count <= 32 &&
           field(player_graphics + 0x1a80, entries) && address >= entries &&
           address - entries < uintptr_t(count) * 0x620 && (address - entries) % 0x620 == 0;
}
struct Operation {
    bool selected;
    Operation(void *object, bool reconstruct, bool component = false)
        : selected(component ? player_component(object)
                             : reinterpret_cast<uintptr_t>(object) == player_graphics) {
        if (selected)
            graphics_operation_enter(lifetime, reconstruct);
    }
    ~Operation() {
        if (selected)
            graphics_operation_leave(lifetime);
    }
};
static void WINAPI clear(void *object) {
    Operation scope(object, true);
    clear_original(object);
}
static void WINAPI load(void *object) {
    Operation scope(object, true);
    load_original(object);
}
static void *WINAPI construct(void *object) {
    Operation scope(object, true);
    return construct_original(object);
}
static void WINAPI destroy(void *object) {
    Operation scope(object, true);
    destroy_original(object);
}
static void WINAPI publish(void *object, void *job) {
    Operation scope(object, true, true);
    publish_original(object, job);
}
static void WINAPI component_clear(void *object) {
    Operation scope(object, true, true);
    component_clear_original(object);
}
static void WINAPI component_load(void *object, void *a, void *b, void *c) {
    Operation scope(object, true, true);
    component_load_original(object, a, b, c);
}
static void WINAPI impact(void *object, float time, void *position, void *force) {
    Operation scope(object, false);
    impact_original(object, time, position, force);
}
static void WINAPI repair(void *object) {
    Operation scope(object, false);
    repair_original(object);
}
static void WINAPI reset(void *object, unsigned char mode) {
    Operation scope(object, false);
    reset_original(object, mode);
}

bool graphics_lifetime_hooks_create(uintptr_t module, uintptr_t graphics, GraphicsLifetime *state) {
    if (!module || !graphics || !state || lifetime)
        return false;
    player_graphics = graphics;
    lifetime = state;
    struct Target {
        uintptr_t rva;
        const char *fixture;
        void *hook;
        void **original;
    };
    const Target targets[]{
        {0xf201c0, "FixtureGraphicsClear", reinterpret_cast<void *>(clear),
         reinterpret_cast<void **>(&clear_original)},
        {0xf20440, "FixtureGraphicsLoad", reinterpret_cast<void *>(load),
         reinterpret_cast<void **>(&load_original)},
        {0xf2c060, "FixtureGraphicsConstruct", reinterpret_cast<void *>(construct),
         reinterpret_cast<void **>(&construct_original)},
        {0xf34fb0, "FixtureGraphicsDestroy", reinterpret_cast<void *>(destroy),
         reinterpret_cast<void **>(&destroy_original)},
        {0xad7410, "FixtureGraphicsPublish", reinterpret_cast<void *>(publish),
         reinterpret_cast<void **>(&publish_original)},
        {0xad71c0, "FixtureGraphicsComponentClear", reinterpret_cast<void *>(component_clear),
         reinterpret_cast<void **>(&component_clear_original)},
        {0xad7930, "FixtureGraphicsComponentLoad", reinterpret_cast<void *>(component_load),
         reinterpret_cast<void **>(&component_load_original)},
        {0xf325c0, "FixtureGraphicsImpact", reinterpret_cast<void *>(impact),
         reinterpret_cast<void **>(&impact_original)},
        {0xf3e930, "FixtureGraphicsRepair", reinterpret_cast<void *>(repair),
         reinterpret_cast<void **>(&repair_original)},
        {0xf35bf0, "FixtureGraphicsReset", reinterpret_cast<void *>(reset),
         reinterpret_cast<void **>(&reset_original)},
    };
    for (const auto &entry : targets) {
#ifdef LMU_RESET_HOST_FIXTURE
        auto target = reinterpret_cast<void *>(
            GetProcAddress(reinterpret_cast<HMODULE>(module), entry.fixture));
#else
        auto target = reinterpret_cast<void *>(module + entry.rva);
#endif
        if (!target || MH_CreateHook(target, entry.hook, entry.original) != MH_OK)
            return false;
    }
    return true;
}
