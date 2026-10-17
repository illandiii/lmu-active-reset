// Read-only LMU SDK consumer. The only writable mapping is the SDK lock protocol.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <utility>
#include <sstream>
#include <string>
#include "SharedMemoryInterface.hpp"

static_assert(sizeof(SharedMemoryLayout) == 324824 && sizeof(TelemInfoV01) == 1888 &&
                  sizeof(VehicleScoringInfoV01) == 584,
              "Unsupported LMU SDK layout");
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
static void number(std::ostream &out, double value) {
    if (std::isfinite(value))
        out << value;
    else
        out << "null";
}
template <size_t N> static void text(std::ostream &out, const char (&value)[N]) {
    size_t length = 0;
    while (length < N && value[length])
        ++length;
    // Prefer UTF-8 names; legacy local encodings are normalized before JSON output.
    UINT encoding = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    int count = MultiByteToWideChar(encoding, flags, value, int(length), nullptr, 0);
    if (!count && length) {
        encoding = CP_ACP;
        flags = 0;
        count = MultiByteToWideChar(encoding, flags, value, int(length), nullptr, 0);
    }
    std::wstring wide(size_t(count), L'\0');
    if (count)
        MultiByteToWideChar(encoding, flags, value, int(length), wide.data(), count);
    int size =
        count ? WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, nullptr, 0, nullptr, nullptr)
              : 0;
    std::string utf8(size_t(size), '\0');
    if (size)
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, utf8.data(), size, nullptr, nullptr);
    out << '"';
    for (unsigned char c : utf8) {
        if (c == '"' || c == '\\')
            out << '\\' << char(c);
        else if (c < 32) {
            char escaped[7];
            snprintf(escaped, sizeof escaped, "\\u%04x", c);
            out << escaped;
        } else
            out << char(c);
    }
    out << '"';
}

struct Frame {
    long version;
    ScoringInfoV01 scoring;
    VehicleScoringInfoV01 cars[104];
    TelemInfoV01 telemetry;
    bool player = false;
};
static bool copyFrame(Frame &f, const SharedMemoryObjectOut &data) {
    f.version = data.generic.gameVersion;
    if (f.version != 14200)
        return false;
    f.scoring = data.scoring.scoringInfo;
    auto count = f.scoring.mNumVehicles;
    if (count < 0 || count > 104 || data.telemetry.activeVehicles > 104)
        return false;
    memcpy(f.cars, data.scoring.vehScoringInfo, size_t(count) * sizeof f.cars[0]);
    const auto &t = data.telemetry;
    if (t.playerHasVehicle && t.playerVehicleIdx < t.activeVehicles) {
        const auto &candidate = t.telemInfo[t.playerVehicleIdx];
        int matches = 0;
        for (long i = 0; i < count; ++i)
            if (f.cars[i].mIsPlayer && f.cars[i].mID == candidate.mID)
                ++matches;
        if (matches == 1) {
            f.telemetry = candidate;
            f.player = true;
        }
    }
    return true;
}
static std::string serialize(const Frame &f, bool foreground) {
    const auto &s = f.scoring;
    const auto &t = f.telemetry;
    std::ostringstream o;
    o.precision(10);
    o << "{\"schema\":1,\"connected\":true,\"foreground\":" << (foreground ? "true" : "false")
      << ",\"driving\":" << (s.mInRealtime && f.player ? "true" : "false")
      << ",\"session\":" << s.mSession << ",\"track\":";
    text(o, s.mTrackName);
    auto n = [&](const char *key, double value) {
        o << ",\"" << key << "\":";
        number(o, value);
    };
    n("et", s.mCurrentET);
    n("end", s.mEndET);
    n("remaining", s.mSessionTimeRemaining);
    n("length", s.mLapDist);
    n("air", s.mAmbientTemp);
    n("trackTemp", s.mTrackTemp);
    n("rain", s.mRaining);
    n("wetness", s.mAvgPathWetness);
    n("cloud", s.mDarkCloud);
    n("limitMax", s.mTrackLimitsStepsPerPenalty);
    n("limitStep", s.mTrackLimitsStepsPerPoint);
    o << ",\"player\":";
    if (!f.player)
        o << "null";
    else {
        o << "{\"id\":" << t.mID;
        n("et", t.mElapsedTime);
        n("lap", t.mLapNumber);
        n("lapStart", t.mLapStartET);
        n("throttle", t.mUnfilteredThrottle);
        n("brake", t.mUnfilteredBrake);
        n("clutch", t.mUnfilteredClutch);
        n("steering", t.mUnfilteredSteering);
        n("steeringRange", t.mVisualSteeringWheelRange);
        n("speed", sqrt(t.mLocalVel.x * t.mLocalVel.x + t.mLocalVel.y * t.mLocalVel.y +
                        t.mLocalVel.z * t.mLocalVel.z) *
                       3.6);
        n("gear", t.mGear);
        n("fuel", t.mFuel);
        n("energy", t.mVirtualEnergy);
        n("fuelCapacity", t.mFuelCapacity);
        n("delta", t.mDeltaBest);
        n("limits", t.mTrackLimitsSteps);
        o << ",\"invalid\":" << (t.mLapInvalidated ? "true" : "false");
        o << ",\"frontCompound\":";
        text(o, t.mFrontTireCompoundName);
        o << ",\"rearCompound\":";
        text(o, t.mRearTireCompoundName);
        o << ",\"wheels\":[";
        for (int i = 0; i < 4; ++i) {
            if (i)
                o << ',';
            const auto &w = t.mWheel[i];
            o << "{\"attached\":" << (w.mDetached ? "false" : "true")
              << ",\"flat\":" << (w.mFlat ? "true" : "false");
            n("rotation", w.mRotation);
            n("groundSpeed", w.mLongitudinalGroundVel);
            n("radius", double(w.mStaticUndeflectedRadius) / 100.0);
            n("deflection", w.mVerticalTireDeflection);
            n("load", w.mTireLoad);
            n("surface", w.mSurfaceType);
            n("wear", w.mWear);
            n("pressure", w.mPressure);
            n("compoundIndex", w.mCompoundIndex);
            o << ",\"temperatures\":[";
            for (int j = 0; j < 3; ++j) {
                if (j)
                    o << ',';
                // The SDK reports left/centre/right tread samples in Kelvin.
                number(o, w.mTemperature[j] > 0 ? w.mTemperature[j] - 273.15 : NAN);
            }
            o << "]}";
        }
        o << "]}";
    }
    o << ",\"cars\":[";
    for (long i = 0; i < s.mNumVehicles; ++i) {
        if (i)
            o << ',';
        const auto &v = f.cars[i];
        o << "{\"id\":" << v.mID << ",\"name\":";
        text(o, v.mDriverName);
        o << ",\"vehicle\":";
        text(o, v.mVehicleName);
        o << ",\"class\":";
        text(o, v.mVehicleClass);
        o << ",\"player\":" << (v.mIsPlayer ? "true" : "false")
          << ",\"pit\":" << (v.mInPits ? "true" : "false");
        n("place", v.mPlace);
        n("laps", v.mTotalLaps);
        n("distance", v.mLapDist);
        n("last", v.mLastLapTime);
        n("best", v.mBestLapTime);
        n("behind", v.mTimeBehindLeader);
        n("lapsBehind", v.mLapsBehindLeader);
        n("lapStart", v.mLapStartET);
        n("estimated", v.mEstimatedLapTime);
        n("pitStops", v.mNumPitstops);
        n("qualification", v.mQualification);
        n("countLap", v.mCountLapFlag);
        o << '}';
    }
    o << "]}";
    return o.str();
}
int main(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "--self-test") || !strcmp(argv[1], "--test-frame"))) {
        auto *data = new SharedMemoryObjectOut{};
        Frame f{};
        data->generic.gameVersion = 14200;
        bool ok = copyFrame(f, *data) && !f.player;
        data->scoring.scoringInfo.mNumVehicles = 105;
        ok = ok && !copyFrame(f, *data);
        data->scoring.scoringInfo.mNumVehicles = 1;
        data->scoring.vehScoringInfo[0].mIsPlayer = true;
        data->scoring.vehScoringInfo[0].mID = 7;
        data->telemetry.activeVehicles = 1;
        data->telemetry.playerHasVehicle = true;
        data->telemetry.telemInfo[0].mID = 8;
        Frame mismatch{};
        ok = ok && copyFrame(mismatch, *data) && !mismatch.player;
        data->telemetry.telemInfo[0].mID = 7;
        Frame match{};
        ok = ok && copyFrame(match, *data) && match.player;
        match.telemetry.mWheel[0].mTemperature[0] = 371.15;
        match.telemetry.mWheel[0].mTemperature[1] = 366.15;
        match.telemetry.mWheel[0].mTemperature[2] = 361.15;
        match.telemetry.mWheel[0].mWear = 0.71;
        match.telemetry.mWheel[0].mStaticUndeflectedRadius = 31;
        match.telemetry.mWheel[0].mVerticalTireDeflection = 0.01;
        match.telemetry.mWheel[0].mLongitudinalGroundVel = 30;
        match.telemetry.mWheel[0].mRotation = 45;
        match.telemetry.mWheel[0].mTireLoad = 3000;
        match.telemetry.mUnfilteredBrake = 0.84;
        match.scoring.mInRealtime = true;
        strcpy(match.telemetry.mFrontTireCompoundName, "Soft");
        const auto json = serialize(match, false);
        ok = ok && json.find("\"temperatures\":[98,93,88]") != std::string::npos &&
             json.find("\"radius\":0.31") != std::string::npos;
        std::ostringstream encoded;
        text(encoded, "Ren\xc3\xa9\n");
        ok = ok && encoded.str() == "\"Ren\xc3\xa9\\u000a\"";
        delete data;
        if (!strcmp(argv[1], "--test-frame")) {
            puts(json.c_str());
            return ok ? 0 : 1;
        }
        puts(ok ? "PASS overlay SDK bounds and player identity" : "FAIL");
        return ok ? 0 : 1;
    }
    if (argc != 3)
        return 2;
    DWORD pid = strtoul(argv[1], nullptr, 10), owner = strtoul(argv[2], nullptr, 10);
    if (!pid || !owner)
        return 2;
    Handle game{OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)},
        parent{OpenProcess(SYNCHRONIZE, FALSE, owner)};
    if (!game.h || !parent.h)
        return 3;
    char executable[32768];
    DWORD size = sizeof executable;
    if (!QueryFullProcessImageNameA(game.h, 0, executable, &size))
        return 3;
    auto name = strrchr(executable, '\\');
    if (!name || _stricmp(name + 1, "Le Mans Ultimate.exe"))
        return 3;
    Handle mapping{OpenFileMappingA(FILE_MAP_READ, FALSE, "LMU_Data")};
    View data{mapping.h ? MapViewOfFile(mapping.h, FILE_MAP_READ, 0, 0, sizeof(SharedMemoryLayout))
                        : nullptr};
    Handle lock{
        OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, "LMU_SharedMemoryLockData")};
    View words{lock.h
                   ? MapViewOfFile(lock.h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(LockWords))
                   : nullptr};
    Handle event{OpenEventA(EVENT_MODIFY_STATE, FALSE, "LMU_SharedMemoryLockEvent")};
    if (!data.p || !words.p || !event.h)
        return 4;
    while (WaitForSingleObject(game.h, 0) == WAIT_TIMEOUT &&
           WaitForSingleObject(parent.h, 0) == WAIT_TIMEOUT) {
        DWORD available = 0;
        if (!PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE), nullptr, 0, nullptr, &available,
                           nullptr) ||
            available)
            break;
        Frame f{};
        bool copied = false;
        {
            Lease lease(static_cast<LockWords *>(words.p), event.h);
            if (lease.held)
                copied = copyFrame(f, static_cast<const SharedMemoryLayout *>(data.p)->data);
        }
        if (copied) {
            DWORD foreground = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &foreground);
            auto line = serialize(f, foreground == pid);
            if (puts(line.c_str()) == EOF || fflush(stdout))
                break;
        }
        Sleep(33);
    }
    return 0;
}
