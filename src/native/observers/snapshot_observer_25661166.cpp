/* Native BallJoint post-call observer; no writes to the vehicle or constraint. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <cmath>
#include <cstring>
#include "snapshot_observer_25661166.h"
#include "MinHook.h"
static PoseIPC *ipc;
static uintptr_t module_base;
static void(WINAPI *original_dynamics)(void *, double);
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
    return read_field(module_base + 0x1e194c8, owner) && read_field(owner + 0x159a8, p) ? p : 0;
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

static bool capture(uintptr_t p, SnapshotFrame &f) {
    uintptr_t bodies[64], constraints[64];
    size_t roots = 0, nc = 0;
    if (player() != p || !vector(p + 0x23f8, bodies, roots) || roots > 28 ||
        !vector(p + 0x2410, constraints, nc))
        return false;
    for (int w = 0; w < 4; w++) {
        uintptr_t tire = 0;
        if (!read_field(p + 0x618 + w * 0x6b0 + 0x3e8, tire) || tire < 0x10000)
            return false;
        bodies[roots + w] = tire + 0xac0;
    }
    f.root_count = roots;
    f.total_count = roots + 4;
    f.constraint_count = nc;
    RigidMassProperties mass[32];
    RigidPreparedBody prepared[32];
    for (size_t i = 0; i < f.total_count; i++) {
        for (size_t j = 0; j < i; j++)
            if (bodies[j] == bodies[i])
                return false;
        if (!body(bodies[i], f.bodies[i]))
            return false;
        mass[i] = f.bodies[i].mass;
    }
    if (rigid_prepare_restore(f.bodies, mass, f.total_count, prepared))
        return false;
    for (size_t i = 0; i < f.total_count; i++) {
        double iw[6], omega[3];
        if (!read_field(bodies[i] + 0x60, iw) || !read_field(bodies[i] + 0xe0, omega))
            return false;
        for (int j = 0; j < 6; j++) {
            if (!std::isfinite(iw[j]))
                return false;
            double e = std::fabs(iw[j] - prepared[i].inverse_inertia_world[j]);
            if (e > f.inertia_error)
                f.inertia_error = e;
        }
        for (int j = 0; j < 3; j++) {
            if (!std::isfinite(omega[j]))
                return false;
            double e = std::fabs(omega[j] - prepared[i].angular_velocity_world[j]);
            if (e > f.omega_error)
                f.omega_error = e;
        }
    }
    for (size_t i = 0; i < nc; i++) {
        uintptr_t c = constraints[i], vt = 0, pair[2];
        auto &r = f.recipes[i];
        auto &m = f.memory[i];
        for (size_t j = 0; j < i; j++)
            if (constraints[j] == c)
                return false;
        if (!read_field(c, vt))
            return false;
        size_t link = 0, axis = 0, stride = 0, soft = 0, impulse = 0, lower = 0, upper = 0,
               rows = 0;
        if (vt == module_base + 0x1807698) {
            r.kind = CONSTRAINT_BAR;
            link = 0xa0;
            rows = 1;
            soft = 0x118;
            impulse = 0x120;
            lower = 0x128;
            upper = 0x130;
        } else if (vt == module_base + 0x1807ba8) {
            r.kind = CONSTRAINT_BALL;
            link = 0x98;
            rows = 3;
            stride = 0xf0;
            soft = 0x110;
            impulse = 0x118;
            lower = 0x120;
            upper = 0x128;
        } else if (vt == module_base + 0x1807d18) {
            r.kind = CONSTRAINT_HINGE;
            link = 0x98;
            rows = 2;
            axis = 0xa8;
            stride = 0xa8;
            soft = 0xf8;
            impulse = 0x100;
            lower = 0x108;
            upper = 0x110;
        } else if (vt == module_base + 0x1808098) {
            r.kind = CONSTRAINT_ORIENTATION;
            link = 0x30;
            rows = 3;
            axis = 0x60;
            stride = 0xe8;
            soft = 0xd0;
            impulse = 0xd8;
            lower = 0xe0;
            upper = 0xe8;
        } else
            return false;
        if (!read_field(c + link, pair))
            return false;
        for (int side = 0; side < 2; side++) {
            size_t matches = 0;
            for (size_t j = 0; j < roots; j++)
                if (pair[side] == bodies[j]) {
                    r.body_indices[side] = m.body_indices[side] = j;
                    ++matches;
                }
            if (matches != 1)
                return false;
        }
        if (pair[0] == pair[1])
            return false;
        r.identity = m.identity = i;
        m.kind = r.kind;
        m.row_count = rows;
        if (r.kind == CONSTRAINT_BAR) {
            if (!read_field(c + 0x50, r.geometry.bar.anchors_local) ||
                !read_field(c + soft, r.geometry.bar.softness))
                return false;
        } else if (r.kind == CONSTRAINT_BALL) {
            if (!read_field(c + 0x50, r.geometry.ball.anchors_local))
                return false;
        }
        for (size_t j = 0; j < rows; j++) {
            auto &row = m.rows[j];
            uintptr_t q = c + j * stride;
            if (r.kind == CONSTRAINT_BALL && !read_field(q + soft, r.geometry.ball.softness[j]))
                return false;
            if (r.kind >= CONSTRAINT_HINGE &&
                (!read_field(q + axis, r.geometry.angular.axes_local[j]) ||
                 !read_field(q + soft, r.geometry.angular.softness[j])))
                return false;
            if (!read_field(q + impulse, row.impulse) || !read_field(q + lower, row.lower) ||
                !read_field(q + upper, row.upper))
                return false;
            if (!std::isfinite(row.impulse) || !std::isfinite(row.lower) ||
                !std::isfinite(row.upper) || row.lower > row.upper)
                return false;
        }
    }
    ConstraintPrepared geometry[64];
    f.geometry_status = constraint_prepare_group(f.bodies, f.total_count, f.recipes, nc, geometry);
    f.memory_valid = 1;
    return player() == p;
}
static thread_local bool active;
static void WINAPI dynamics(void *object, double time) {
    if (active || !lease() || player() != (uintptr_t)object || !std::isfinite(time)) {
        original_dynamics(object, time);
        return;
    }
    LONG64 serial = InterlockedIncrement64(&ipc->callbacks);
    if (serial % 32) {
        original_dynamics(object, time);
        return;
    }
    LONG64 tid = GetCurrentThreadId(),
           prior = InterlockedCompareExchange64(&ipc->writer_thread, tid, 0);
    if (prior && prior != tid) {
        InterlockedIncrement64(&ipc->skipped);
        original_dynamics(object, time);
        return;
    }
    active = true;
    SnapshotFrame f{};
    LARGE_INTEGER begin, end;
    QueryPerformanceCounter(&begin);
    if (capture((uintptr_t)object, f) && lease()) {
        QueryPerformanceCounter(&end);
        f.tick_ms = GetTickCount64();
        f.simulation_time = time;
        f.observer_us = (end.QuadPart - begin.QuadPart) * 1e6 / ipc->qpc_frequency;
        InterlockedIncrement64(&ipc->sequence);
        ipc->frame = f;
        MemoryBarrier();
        InterlockedIncrement64(&ipc->sequence);
        InterlockedIncrement64(&ipc->observations);
    } else
        InterlockedIncrement64(&ipc->skipped);
    original_dynamics(object, time);
    active = false;
}
static int check_game_hash(void) {
#ifdef LMU_POSE_FIXTURE
    return 1;
#else
    static const unsigned char expected[32] = {0x66, 0x94, 0x23, 0x37, 0x81, 0x3b, 0x30, 0x1b,
                                               0x87, 0xd1, 0x2e, 0xdf, 0xa3, 0x30, 0xf3, 0x33,
                                               0x54, 0xa2, 0xa4, 0xb8, 0xcd, 0x9b, 0x04, 0x88,
                                               0x85, 0xcb, 0xf7, 0x7e, 0xb6, 0xf9, 0x40, 0xcf};
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
    targets[0] = (void *)GetProcAddress((HMODULE)module_base, "FixtureDynamics");
#else
    targets[0] = (void *)(module_base + 0xecb4e0);
    const unsigned char prolog[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48,
                                    0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20};
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
    void *hooks[1] = {(void *)&dynamics};
    void **trampolines[1] = {(void **)&original_dynamics};
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
