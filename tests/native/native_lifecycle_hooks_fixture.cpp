#include "../../src/native/runtime/vehicle_lifecycle_hooks.h"
#include "MinHook.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

static NativeLifecycleEpoch epoch;
static unsigned calls[7];
static int object;
static void check(void *p, unsigned index) {
    assert(p == &object && epoch.mutations && epoch.generation > 1);
    NativeLifecyclePin pin{};
    assert(!native_lifecycle_try_pin(&epoch, epoch.generation, &pin));
    ++calls[index];
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureClear(void *p,
                                                                               unsigned char flag) {
    check(p, 0);
    assert(flag == 0xa5);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureDestroy(void *p) {
    check(p, 2);
    void(WINAPI *volatile nested)(void *, unsigned char) = FixtureClear;
    nested(p, 0xa5);
}
extern "C" __declspec(dllexport) __declspec(noinline) void *WINAPI FixtureConstruct(void *p) {
    check(p, 3);
    return p;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureReconstruct(void *p) {
    check(p, 1);
    void(WINAPI *volatile destroy)(void *) = FixtureDestroy;
    void *(WINAPI *volatile construct)(void *) = FixtureConstruct;
    destroy(p);
    assert(epoch.mutations && construct(p) == p);
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureVehicleDestroy(void *p) {
    check(p, 4);
}
extern "C" __declspec(dllexport) __declspec(noinline) void *WINAPI
FixtureVehicleConstruct(void *p) {
    check(p, 5);
    return p;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureControl(void *p, int mode,
                                                                                 char remote,
                                                                                 char replay) {
    check(p, 6);
    assert(mode == -1 && remote == 0x36 && replay == 0x72);
}
int main() {
    native_lifecycle_init(&epoch);
    assert(MH_Initialize() == MH_OK);
    assert(native_lifecycle_hooks_create(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),
                                         &epoch));
    assert(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
    void(WINAPI *volatile reconstruct)(void *) = FixtureReconstruct;
    void(WINAPI *volatile vehicle_destroy)(void *) = FixtureVehicleDestroy;
    void *(WINAPI *volatile vehicle_construct)(void *) = FixtureVehicleConstruct;
    void(WINAPI *volatile control)(void *, int, char, char) = FixtureControl;
    reconstruct(&object);
    vehicle_destroy(&object);
    assert(vehicle_construct(&object) == &object);
    control(&object, -1, 0x36, 0x72);
    assert(epoch.generation == 8 && epoch.mutations == 0);
    for (auto count : calls)
        assert(count == 1);
    NativeLifecyclePin pin{};
    assert(!native_lifecycle_try_pin(&epoch, 1, &pin));
    assert(native_lifecycle_try_pin(&epoch, 8, &pin));
    assert(native_lifecycle_release(&pin));
    assert(MH_DisableHook(MH_ALL_HOOKS) == MH_OK);
    puts("PASS seven lifecycle hooks: native signatures/returns, nested clear/destruct/construct, "
         "no pin during reconstruction and permanent old-epoch rejection.");
}
