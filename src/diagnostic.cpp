// External diagnostic only. No injection, physics writes, or reset commands.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <optional>
#include <utility>
#include <cstddef>
#include "SharedMemoryInterface.hpp"

static_assert(sizeof(void *) == 8 && sizeof(long) == 4 && sizeof(bool) == 1,
              "Windows x64 ABI required");
static_assert(sizeof(SharedMemoryLayout) == 324824 && sizeof(ScoringInfoV01) == 548 &&
                  sizeof(VehicleScoringInfoV01) == 584 && sizeof(TelemInfoV01) == 1888 &&
                  sizeof(TelemWheelV01) == 260 && sizeof(ApplicationStateV01) == 260,
              "Local build 25081074 SDK layout changed");
static_assert(offsetof(SharedMemoryObjectOut, telemetry) == 128464 &&
                  offsetof(ScoringInfoV01, mGameMode) == 284,
              "Local build offsets changed");

struct Handle {
    HANDLE h = nullptr;
    ~Handle() {
        if (h)
            CloseHandle(h);
    }
};
struct View {
    void *p = nullptr;
    ~View() {
        if (p)
            UnmapViewOfFile(p);
    }
};
struct LockWords {
    volatile LONG waiters;
    volatile LONG busy;
};
// Fast path of the installed SDK's lock protocol. Never create/reset lock data,
// never enter its waiter path, and never touch the LMU_Data payload.
struct Lease {
    LockWords *words;
    HANDLE event;
    bool held;
    Lease(LockWords *w, HANDLE e)
        : words(w), event(e), held(InterlockedCompareExchange(&w->busy, 1, 0) == 0) {}
    ~Lease() {
        if (held) {
            InterlockedExchange(&words->busy, 0);
            if (InterlockedCompareExchange(&words->waiters, 0, 0) > 0)
                SetEvent(event);
        }
    }
};

struct Snapshot {
    uint32_t events[SME_MAX];
    long version, session, vehicles;
    unsigned char location, phase, mode;
    bool realtime, hasVehicle;
    unsigned active, playerIndex;
    double currentET;
    bool playerFound, telemetryFound, boundsOK;
    int playerCount;
    long id;
    signed char control;
    unsigned char countLap;
    short laps;
    bool inPits, inGarage;
    double best, last, lapStart;
    TelemInfoV01 telem; // temporary in RAM; only the explicit allowlist is logged
};

static void copySnapshot(Snapshot &s, const SharedMemoryObjectOut &d) {
    for (unsigned i = 0; i < SME_MAX; ++i)
        s.events[i] = d.generic.events[i];
    s.version = d.generic.gameVersion;
    s.location = d.generic.appInfo.mOptionsLocation;
    const auto &sc = d.scoring.scoringInfo;
    s.session = sc.mSession;
    s.vehicles = sc.mNumVehicles;
    s.currentET = sc.mCurrentET;
    s.phase = sc.mGamePhase;
    s.mode = sc.mGameMode;
    s.realtime = sc.mInRealtime;
    s.active = d.telemetry.activeVehicles;
    s.playerIndex = d.telemetry.playerVehicleIdx;
    s.hasVehicle = d.telemetry.playerHasVehicle;
    s.boundsOK = s.vehicles >= 0 && s.vehicles <= 104 && s.active <= 104 &&
                 (!s.hasVehicle || s.playerIndex < s.active) && d.scoring.scoringStreamSize < 65536;
    if (!s.boundsOK)
        return;
    for (long i = 0; i < s.vehicles; ++i) {
        const auto &v = d.scoring.vehScoringInfo[i];
        if (!v.mIsPlayer)
            continue;
        ++s.playerCount;
        s.id = v.mID;
        s.control = v.mControl;
        s.countLap = v.mCountLapFlag;
        s.laps = v.mTotalLaps;
        s.best = v.mBestLapTime;
        s.last = v.mLastLapTime;
        s.lapStart = v.mLapStartET;
        s.inPits = v.mInPits;
        s.inGarage = v.mInGarageStall;
    }
    s.playerFound = s.playerCount == 1;
    if (s.hasVehicle && s.playerFound) {
        const auto &t = d.telemetry.telemInfo[s.playerIndex];
        if (t.mID == s.id) {
            s.telem = t;
            s.telemetryFound = true;
        }
    }
}

static const char *decision(const Snapshot &s, bool fresh) {
    if (!s.boundsOK || !fresh)
        return "unknown";
    if (s.session < 1 || s.session > 4)
        return "forbidden";
    if (s.playerFound && s.control != 0)
        return "forbidden";
    // No installed-SDK field currently supplies positive offline proof.
    // Intentionally no production path to allowed and no user override.
    return "unknown";
}

static void number(FILE *f, const char *key, double n) {
    fprintf(f, ",\"%s\":", key);
    if (std::isfinite(n))
        fprintf(f, "%.10g", n);
    else
        fputs("null", f);
}
static void vector(FILE *f, const char *key, const TelemVect3 &v) {
    if (std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z))
        fprintf(f, ",\"%s\":[%.10g,%.10g,%.10g]", key, v.x, v.y, v.z);
    else
        fprintf(f, ",\"%s\":null", key);
}
static void schema() {
    printf("{\"pointer\":%zu,\"long\":%zu,\"bool\":%zu,", sizeof(void *), sizeof(long),
           sizeof(bool));
    printf("\"SharedMemoryLayout\":%zu,\"SharedMemoryGeneric\":%zu,", sizeof(SharedMemoryLayout),
           sizeof(SharedMemoryGeneric));
    printf("\"ScoringInfoV01\":%zu,\"VehicleScoringInfoV01\":%zu,", sizeof(ScoringInfoV01),
           sizeof(VehicleScoringInfoV01));
    printf("\"TelemInfoV01\":%zu,\"TelemWheelV01\":%zu,\"ApplicationStateV01\":%zu,",
           sizeof(TelemInfoV01), sizeof(TelemWheelV01), sizeof(ApplicationStateV01));
    printf("\"scoring_offset\":%zu,\"telemetry_offset\":%zu,",
           offsetof(SharedMemoryObjectOut, scoring), offsetof(SharedMemoryObjectOut, telemetry));
    printf("\"session_offset\":%zu,\"game_mode_offset\":%zu,\"lap_invalidated_offset\":%zu}\n",
           offsetof(ScoringInfoV01, mSession), offsetof(ScoringInfoV01, mGameMode),
           offsetof(TelemInfoV01, mLapInvalidated));
}

static int selftest() {
    int failed = 0, tests = 0;
    auto check = [&](bool ok, const char *name) {
        ++tests;
        if (!ok)
            ++failed;
        printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    };
    Snapshot s{};
    s.boundsOK = true;
    s.playerFound = true;
    for (int session = 0; session <= 13; ++session) {
        s.session = session;
        check(strcmp(decision(s, true), session >= 1 && session <= 4 ? "unknown" : "forbidden") ==
                  0,
              "session gate");
    }
    s.session = 1;
    check(strcmp(decision(s, false), "unknown") == 0, "stale data refused");
    s.control = 2;
    check(strcmp(decision(s, true), "forbidden") == 0, "remote control refused");
    s.control = 0;
    s.boundsOK = false;
    check(strcmp(decision(s, true), "unknown") == 0, "bad schema bounds refused");
    LockWords words{0, 0};
    {
        Lease first(&words, nullptr);
        check(first.held, "lock acquire");
        {
            Lease second(&words, nullptr);
            check(!second.held, "contention skips");
        }
        check(words.busy == 1, "failed acquisition does not unlock owner");
    }
    check(words.busy == 0, "owner unlocks");
    auto *d = new SharedMemoryObjectOut{};
    d->scoring.scoringInfo.mNumVehicles = 105;
    Snapshot bad{};
    copySnapshot(bad, *d);
    check(!bad.boundsOK, "vehicle bounds before array access");
    d->scoring.scoringInfo.mNumVehicles = 1;
    d->scoring.vehScoringInfo[0].mIsPlayer = true;
    d->scoring.vehScoringInfo[0].mID = 7;
    d->telemetry.activeVehicles = 1;
    d->telemetry.playerHasVehicle = true;
    d->telemetry.playerVehicleIdx = 0;
    d->telemetry.telemInfo[0].mID = 8;
    Snapshot mismatch{};
    copySnapshot(mismatch, *d);
    check(!mismatch.telemetryFound, "mismatching scoring and telemetry IDs refused");
    d->telemetry.playerVehicleIdx = 1;
    Snapshot badIndex{};
    copySnapshot(badIndex, *d);
    check(!badIndex.boundsOK, "player index bound");
    delete d;
    printf("RESULT tests=%d failed=%d\n", tests, failed);
    return failed ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--schema")) {
        schema();
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--self-test"))
        return selftest();
    if (argc != 4) {
        fprintf(stderr, "Usage: diagnostic.exe PID SECONDS LOG.jsonl | --schema | --self-test\n");
        return 2;
    }
    char *end = nullptr;
    unsigned long pid = strtoul(argv[1], &end, 10);
    if (!pid || *end)
        return 2;
    unsigned long seconds = strtoul(argv[2], &end, 10);
    if (!seconds || seconds > 3600 || *end)
        return 2;
    Handle parent{OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)};
    if (!parent.h) {
        fprintf(stderr, "OpenProcess failed: %lu\n", GetLastError());
        return 3;
    }
    Handle mapping{OpenFileMappingA(FILE_MAP_READ, FALSE, "LMU_Data")};
    if (!mapping.h) {
        fprintf(stderr, "LMU_Data unavailable: %lu\n", GetLastError());
        return 4;
    }
    View data{MapViewOfFile(mapping.h, FILE_MAP_READ, 0, 0, sizeof(SharedMemoryLayout))};
    if (!data.p) {
        fprintf(stderr, "LMU_Data size/map failed: %lu\n", GetLastError());
        return 4;
    }
    Handle lockMap{
        OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, "LMU_SharedMemoryLockData")};
    if (!lockMap.h) {
        fprintf(stderr, "Existing SDK lock unavailable: %lu\n", GetLastError());
        return 5;
    }
    View lockView{
        MapViewOfFile(lockMap.h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(LockWords))};
    Handle lockEvent{OpenEventA(EVENT_MODIFY_STATE, FALSE, "LMU_SharedMemoryLockEvent")};
    if (!lockView.p || !lockEvent.h) {
        fprintf(stderr, "SDK lock mapping/event failed: %lu\n", GetLastError());
        return 5;
    }
    FILE *f = fopen(argv[3], "wb");
    if (!f) {
        perror("log");
        return 6;
    }
    LARGE_INTEGER freq{}, begin{}, now{}, lockStart{}, lockEnd{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&begin);
    double prevScoringET = 0, prevTelemetryET = 0;
    bool seenScoring = false, seenTelemetry = false;
    double scoringChanged = 0, telemetryChanged = 0, maxHold = 0;
    unsigned samples = 0, skipped = 0;
    for (;;) {
        QueryPerformanceCounter(&now);
        double elapsed = double(now.QuadPart - begin.QuadPart) / freq.QuadPart;
        if (elapsed >= seconds || WaitForSingleObject(parent.h, 0) != WAIT_TIMEOUT)
            break;
        Snapshot s{}; // allocation and zeroing outside the producer lock
        bool copied = false;
        QueryPerformanceCounter(&lockStart);
        {
            Lease lease(static_cast<LockWords *>(lockView.p), lockEvent.h);
            if (lease.held) {
                copySnapshot(s, static_cast<const SharedMemoryLayout *>(data.p)->data);
                copied = true;
            }
        }
        QueryPerformanceCounter(&lockEnd);
        if (!copied) {
            ++skipped;
            Sleep(20);
            continue;
        }
        double holdUs = double(lockEnd.QuadPart - lockStart.QuadPart) * 1e6 / freq.QuadPart;
        if (holdUs > maxHold)
            maxHold = holdUs;
        // The observed events array is NOT a monotonic per-event counter.
        // Freshness here is only evidence of advancing simulation timestamps.
        // A paused/stationary timestamp becomes unknown; never infer session identity.
        if (samples > 0 && std::isfinite(s.currentET) && s.currentET > prevScoringET) {
            seenScoring = true;
            scoringChanged = elapsed;
        }
        if (samples > 0 && s.telemetryFound && std::isfinite(s.telem.mElapsedTime) &&
            s.telem.mElapsedTime > prevTelemetryET) {
            seenTelemetry = true;
            telemetryChanged = elapsed;
        }
        if (s.currentET < prevScoringET ||
            (s.telemetryFound && s.telem.mElapsedTime < prevTelemetryET)) {
            seenScoring = false;
            seenTelemetry = false;
        }
        prevScoringET = s.currentET;
        prevTelemetryET = s.telem.mElapsedTime;
        bool fresh = seenScoring && seenTelemetry && elapsed - scoringChanged < 1 &&
                     elapsed - telemetryChanged < 1;
        fprintf(f,
                "{\"sample\":%u,\"host_tick_ms\":%llu,\"elapsed_s\":%.6f,\"lock_copy_us\":%.3f,"
                "\"game_version\":%ld,\"session\":%ld,\"game_mode\":%u,\"location\":%u,\"phase\":%"
                "u,\"in_realtime\":%s,\"vehicles\":%ld,\"active_vehicles\":%u,\"bounds_ok\":%s,"
                "\"fresh_observed\":%s,\"reset_decision\":\"%s\",\"offline_proof\":\"not_checked_"
                "by_native_reader\",\"events\":[",
                samples, static_cast<unsigned long long>(GetTickCount64()), elapsed, holdUs,
                s.version, s.session, s.mode, s.location, s.phase, s.realtime ? "true" : "false",
                s.vehicles, s.active, s.boundsOK ? "true" : "false", fresh ? "true" : "false",
                decision(s, fresh));
        for (unsigned i = 0; i < SME_MAX; ++i)
            fprintf(f, "%s%u", i ? "," : "", s.events[i]);
        fprintf(f, "],\"player_found\":%s,\"telemetry_matched\":%s",
                s.playerFound ? "true" : "false", s.telemetryFound ? "true" : "false");
        number(f, "session_et", s.currentET);
        if (s.playerFound) {
            fprintf(f,
                    ",\"player_id\":%ld,\"control\":%d,\"total_laps\":%d,\"count_lap_flag\":%u,"
                    "\"in_pits\":%s,\"in_garage\":%s",
                    s.id, s.control, s.laps, s.countLap, s.inPits ? "true" : "false",
                    s.inGarage ? "true" : "false");
            number(f, "best_lap_s", s.best);
            number(f, "last_lap_s", s.last);
            number(f, "scoring_lap_start_et", s.lapStart);
        }
        if (s.telemetryFound) {
            const auto &t = s.telem;
            fprintf(f, ",\"gear\":%ld,\"lap_number\":%ld,\"lap_invalidated\":%s", t.mGear,
                    t.mLapNumber, t.mLapInvalidated ? "true" : "false");
            number(f, "telemetry_et", t.mElapsedTime);
            number(f, "rpm", t.mEngineRPM);
            number(f, "fuel_l", t.mFuel);
            number(f, "battery_fraction", t.mBatteryChargeFraction);
            number(f, "virtual_energy", t.mVirtualEnergy);
            number(f, "throttle", t.mUnfilteredThrottle);
            number(f, "brake", t.mUnfilteredBrake);
            number(f, "steering", t.mUnfilteredSteering);
            number(f, "clutch", t.mUnfilteredClutch);
            vector(f, "position_m", t.mPos);
            vector(f, "velocity_local_mps", t.mLocalVel);
            vector(f, "rotation_local_rps", t.mLocalRot);
            vector(f, "orientation_0", t.mOri[0]);
            vector(f, "orientation_1", t.mOri[1]);
            vector(f, "orientation_2", t.mOri[2]);
        }
        fputs("}\n", f);
        fflush(f);
        ++samples;
        Sleep(100); // polling avoids consuming the shared producer event
    }
    fclose(f);
    printf("samples=%u contention_skips=%u max_lock_copy_us=%.3f\n", samples, skipped, maxHold);
    return samples ? 0 : 7;
}
