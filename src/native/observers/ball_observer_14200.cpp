/* Native BallJoint post-call observer; no writes to the vehicle or constraint. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <cmath>
#include <cstring>
#include "ball_observer_ipc_14200.h"
#include "MinHook.h"
static PoseIPC *ipc;
static uintptr_t module_base;
static void(WINAPI *original_ball)(void *);
static void *targets[1];
static unsigned char originals[1][32];
template <class T> static bool read_field(uintptr_t p, T &out) {
    SIZE_T got = 0;
    return p >= 0x10000 && p < 0x800000000000ULL &&
           ReadProcessMemory(GetCurrentProcess(), (void *)p, &out, sizeof(out), &got) &&
           got == sizeof(out);
}
static bool lease() {
    LONG64 now = GetTickCount64(), hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
    return !ipc->stop && ipc->allow_observation == 1 && hb <= now && now - hb <= 300;
}
static uintptr_t player() {
#ifdef LMU_POSE_FIXTURE
    typedef void *(WINAPI * F)();
    auto f = (F)GetProcAddress((HMODULE)module_base, "FixturePlayer");
    return f ? (uintptr_t)f() : 0;
#else
    uintptr_t owner = 0, p = 0;
    return read_field(module_base + 0x1e161e8, owner) && read_field(owner + 0x159a0, p) ? p : 0;
#endif
}
static bool vector(uintptr_t p, uintptr_t *out, size_t &count) {
    uintptr_t v[3];
    if (!read_field(p, v) || v[0] < 0x10000 || v[1] < v[0] || v[2] < v[1] || (v[1] - v[0]) % 8 ||
        (v[2] - v[0]) % 8)
        return false;
    count = (v[1] - v[0]) / 8;
    if (!count || count > 64)
        return false;
    for (size_t i = 0; i < count; i++)
        if (!read_field(v[0] + 8 * i, out[i]))
            return false;
    return true;
}
static bool body(uintptr_t p, RigidSavedBody &b) {
    return read_field(p + 0x58, b.mass.inverse_mass) &&
           read_field(p + 0xf8, b.mass.inverse_inertia_local) &&
           read_field(p + 0x40, b.position_world) && read_field(p + 0x28, b.velocity_world) &&
           read_field(p + 0x90, b.orientation) && read_field(p + 0x110, b.momentum_world);
}
static bool capture(uintptr_t c, BallFrame &f) {
    uintptr_t p = player(), constraints[64], roots[64];
    size_t nc = 0, nr = 0;
    if (!p || !vector(p + 0x2410, constraints, nc) || !vector(p + 0x23f8, roots, nr))
        return false;
    size_t matches = 0;
    for (size_t i = 0; i < nc; i++)
        if (constraints[i] == c) {
            f.constraint_index = i;
            ++matches;
        }
    if (matches != 1)
        return false;
#ifndef LMU_POSE_FIXTURE
    uintptr_t vt = 0;
    if (!read_field(c, vt) || vt != module_base + 0x1804868)
        return false;
#endif
    uintptr_t links[2];
    if (!read_field(c + 0x98, links))
        return false;
    for (int i = 0; i < 2; i++) {
        matches = 0;
        for (size_t j = 0; j < nr; j++)
            if (roots[j] == links[i])
                ++matches;
        if (matches != 1 || !body(links[i], f.bodies[i]) ||
            !read_field(c + 0x50 + i * 0x18, f.geometry.anchors_local[i]))
            return false;
    }

    BarGeometryPrepared out[3];
    for (int axis = 0; axis < 3; axis++) {
        auto &row = f.rows[axis];
        uintptr_t r = c + axis * 0xf0;
        if (!read_field(r + 0x110, f.geometry.softness[axis]) ||
            !read_field(r + 0xa8, row.native_jacobian) ||
            !read_field(r + 0x138, row.native_weighted) ||
            !read_field(r + 0x130, row.native_coefficient) ||
            !read_field(r + 0x118, row.impulse_after) || !read_field(r + 0x120, row.lower) ||
            !read_field(r + 0x128, row.upper))
            return false;
        for (int i = 0; i < 12; i++)
            if (!std::isfinite(row.native_jacobian[i]) || !std::isfinite(row.native_weighted[i]))
                return false;
        if (!std::isfinite(row.native_coefficient) || !std::isfinite(row.impulse_after) ||
            !std::isfinite(row.lower) || !std::isfinite(row.upper))
            return false;
    }
    f.prepare_status = ball_prepare_geometry(f.bodies, &f.geometry, out);
    if (!f.prepare_status)
        for (int axis = 0; axis < 3; axis++) {
            auto &row = f.rows[axis];
            for (int i = 0; i < 12; i++) {
                double e = std::fabs(out[axis].jacobian[i / 3][i % 3] - row.native_jacobian[i]);
                if (e > f.jacobian_error)
                    f.jacobian_error = e;
                e = std::fabs(out[axis].weighted[i / 3][i % 3] - row.native_weighted[i]);
                if (e > f.weighted_error)
                    f.weighted_error = e;
            }
            double e = std::fabs(out[axis].inverse_effective_mass - row.native_coefficient);
            if (e > f.coefficient_error)
                f.coefficient_error = e;
        }
    f.tick_ms = GetTickCount64();
    return player() == p;
}
static bool warm_state(uintptr_t c, BarWarmState &out) {
    uintptr_t links[2];
    if (!read_field(c + 0x98, links))
        return false;
    for (int i = 0; i < 2; i++)
        if (!read_field(links[i] + 0x28, out.velocity[i]) ||
            !read_field(links[i] + 0xe0, out.omega[i]))
            return false;
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 3; j++)
            if (!std::isfinite(out.velocity[i][j]) || !std::isfinite(out.omega[i][j]))
                return false;
    return true;
}
static void WINAPI ball(void *object) {
    LONG64 serial = InterlockedIncrement64(&ipc->callbacks);
    bool sample = serial % 127 == 0 && lease();
    BallFrame f{};
    uintptr_t c = (uintptr_t)object;
    if (sample)
        sample = warm_state(c, f.warm_before);
    if (sample)
        for (int axis = 0; axis < 3; axis++)
            if (!read_field(c + 0x118 + axis * 0xf0, f.rows[axis].impulse_before) ||
                !std::isfinite(f.rows[axis].impulse_before)) {
                sample = false;
                break;
            }
    original_ball(object);
    if (!sample || !lease())
        return;
    LONG64 tid = GetCurrentThreadId(),
           prior = InterlockedCompareExchange64(&ipc->writer_thread, tid, 0);
    if (prior && prior != tid) {
        InterlockedIncrement64(&ipc->skipped);
        return;
    }
    if (!capture(c, f) || !warm_state(c, f.warm_after) || !lease()) {
        InterlockedIncrement64(&ipc->skipped);
        return;
    }
    BarGeometryPrepared g[3];
    BarWarmState expected = f.warm_before;
    f.warm_status = ball_prepare_geometry(f.bodies, &f.geometry, g);
    if (!f.warm_status)
        for (int axis = 0; axis < 3; axis++) {
            auto &row = f.rows[axis];
            BarWarmState next;
            expected.impulse = row.impulse_before;
            f.warm_status =
                bar_prepare_warm_start(g + axis, &expected, row.lower, row.upper, &next);
            if (f.warm_status)
                break;
            double e = std::fabs(next.impulse - row.impulse_after);
            if (e > f.warm_error)
                f.warm_error = e;
            expected = next;
        }
    if (!f.warm_status)
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 3; j++) {
                double e = std::fabs(expected.velocity[i][j] - f.warm_after.velocity[i][j]);
                if (e > f.warm_error)
                    f.warm_error = e;
                e = std::fabs(expected.omega[i][j] - f.warm_after.omega[i][j]);
                if (e > f.warm_error)
                    f.warm_error = e;
            }
    InterlockedIncrement64(&ipc->sequence);
    ipc->frame = f;
    MemoryBarrier();
    InterlockedIncrement64(&ipc->sequence);
    InterlockedIncrement64(&ipc->observations);
}
static int check_game_hash(void) {
#ifdef LMU_POSE_FIXTURE
    return 1;
#else
    static const unsigned char expected[32] = {0xb3, 0x7a, 0x1d, 0xd3, 0xa5, 0x91, 0x23, 0x91,
                                               0xdb, 0x23, 0x60, 0x51, 0x99, 0x5c, 0xaa, 0xdd,
                                               0x6d, 0x55, 0x58, 0x63, 0x1e, 0xd9, 0xce, 0x5d,
                                               0x68, 0xf8, 0x39, 0x7a, 0xbe, 0x2f, 0x6a, 0x8c};
    wchar_t path[32768];
    if (!GetModuleFileNameW(NULL, path, 32768))
        return 0;
    HANDLE file =
        CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    int ok = 0;
    if (CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        unsigned char buffer[65536], digest[32];
        DWORD got = 0, size = 32;
        BOOL read_ok;
        do {
            read_ok = ReadFile(file, buffer, sizeof(buffer), &got, NULL);
            if (!read_ok || (!CryptHashData(hash, buffer, got, 0)))
                break;
        } while (got);
        if (read_ok && got == 0 && CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0) &&
            size == 32)
            ok = memcmp(digest, expected, 32) == 0;
    }
    if (hash)
        CryptDestroyHash(hash);
    if (provider)
        CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok;
#endif
}

static DWORD WINAPI worker(void *) {
    wchar_t name[96];
    pose_mapping_name(name, GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping)
        return 0;
    ipc = (PoseIPC *)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*ipc));
    if (!ipc || ipc->magic != POSE_MAGIC || ipc->version != 1 || ipc->pid != GetCurrentProcessId())
        return 0;
    ipc->status = 1;
    module_base = (uintptr_t)GetModuleHandleW(NULL);
    if (!check_game_hash()) {
        ipc->error = 100;
        ipc->status = 4;
        return 0;
    }
#ifdef LMU_POSE_FIXTURE
    targets[0] = (void *)GetProcAddress((HMODULE)module_base, "FixtureBall");
#else
    targets[0] = (void *)(module_base + 0x3e39d0);
    const unsigned char prolog[] = {0x48, 0x8b, 0xc4, 0x48, 0x81, 0xec, 0xd8, 0x00, 0x00, 0x00};
    if (memcmp(targets[0], prolog, sizeof(prolog))) {
        ipc->error = 101;
        ipc->status = 4;
        return 0;
    }
#endif
    for (int i = 0; i < 1; i++) {
        if (!targets[i]) {
            ipc->error = 105;
            ipc->status = 4;
            return 0;
        }
        memcpy(originals[i], targets[i], 32);
    }
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    ipc->qpc_frequency = freq.QuadPart;
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        ipc->error = 200 + status;
        ipc->status = 4;
        return 0;
    }
    void *hooks[1] = {(void *)&ball};
    void **trampolines[1] = {(void **)&original_ball};
    for (int i = 0; i < 1; i++) {
        status = MH_CreateHook(targets[i], hooks[i], trampolines[i]);
        if (status != MH_OK) {
            ipc->error = 300 + status;
            ipc->status = 4;
            return 0;
        }
    }
    LONG64 now = GetTickCount64(), hb = ipc->heartbeat_ms;
    if (ipc->stop || hb > now || now - hb > 2000) {
        ipc->error = 106;
        ipc->status = 4;
        return 0;
    }
    status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        ipc->error = 400 + status;
        ipc->status = 4;
        return 0;
    }
    ipc->status = 2;
    ULONGLONG started = GetTickCount64();
    while (!ipc->stop) {
        now = GetTickCount64();
        hb = InterlockedCompareExchange64(&ipc->heartbeat_ms, 0, 0);
        if (hb > now || now - hb > 2000 || (ULONGLONG)now - started > 120000)
            break;
        Sleep(20);
    }
    ipc->allow_observation = 0;
    status = MH_DisableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        ipc->error = 500 + status;
        ipc->status = 5;
        return 0;
    }
    for (int i = 0; i < 1; i++)
        if (memcmp(targets[i], originals[i], 32)) {
            ipc->error = 110 + i;
            ipc->status = 5;
            return 0;
        }
    ipc->status = 3;
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (t)
            CloseHandle(t);
    }
    return TRUE;
}
