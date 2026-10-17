#ifndef SECTOR_AERO_RESTORE_25661166_H
#define SECTOR_AERO_RESTORE_25661166_H

#include <stdint.h>

/* Native repair ECF380/F5E63E restores seven coefficients. ED8130,
 * ED8070 and ED7DE0 update the ten dependent aerodynamic caches. */
enum { AERO_PRIMARY_COUNT = 7, AERO_FIELD_COUNT = 17, AERO_CONFIG_COUNT = 33 };
static const uint32_t aero_field_offsets[AERO_FIELD_COUNT] = {
    0x39d8, 0x39f0, 0x3a08, 0x3a20, 0x3a38, 0x3a48, 0x40d8, 0x3c28, 0x3c20,
    0x3bb8, 0x3bc8, 0x3df0, 0x3de8, 0x3d98, 0x3da8, 0x4090, 0x3f60};

#ifdef __cplusplus
#include <cmath>
#include <cstring>

namespace sector_aero {
// Read-only setup dependencies. They must match the checkpoint, so restoring
// cached coefficients cannot silently overwrite a newly selected setup.
static constexpr uint32_t config_offsets[AERO_CONFIG_COUNT] = {
    0x41d0, 0x41d8, 0x41e0, 0x41e8, 0x41f0, 0x41f8, 0x4200, 0x4208, 0x4210, 0x4218, 0x4220,
    0x4228, 0x4230, 0x4238, 0x4240, 0x39e0, 0x39e8, 0x39f8, 0x3a00, 0x3a10, 0x3a18, 0x3a28,
    0x3a30, 0x3a40, 0x3a50, 0x3a58, 0x3a60, 0x3a68, 0x3a70, 0x3bc0, 0x3bd0, 0x3da0, 0x3db0};

struct State {
    double values[AERO_FIELD_COUNT];
    double config[AERO_CONFIG_COUNT];
};

inline void derived(const State &state, double (&out)[10]) {
    const auto &v = state.values;
    const auto &c = state.config;
    const double front = c[0], rear = c[10], body = c[5], other = c[6];
    out[0] = v[0] + front * c[15] + front * front * c[16];
    out[1] = v[1] + front * c[17] + front * front * c[18];
    out[2] = c[3] + (v[0] - c[1]) * (1 - c[29]);
    out[3] = c[4] + (v[1] - c[2]) * (1 - c[30]);
    out[4] = v[2] + rear * c[19] + rear * rear * c[20];
    out[5] = v[3] + rear * c[21] + rear * rear * c[22];
    out[6] = c[13] + (v[2] - c[11]) * (1 - c[31]);
    out[7] = c[14] + (v[3] - c[12]) * (1 - c[32]);
    out[8] = (v[4] + body * c[23]) + (c[25] + other * c[26]);
    out[9] = (v[5] + body * c[24]) + (c[27] + other * c[28]);
}

inline bool coherent(const State &state) {
    for (double value : state.values)
        if (!std::isfinite(value))
            return false;
    for (double value : state.config)
        if (!std::isfinite(value))
            return false;
    double expected[10];
    derived(state, expected);
    for (unsigned i = 0; i < 10; ++i)
        if (!std::isfinite(expected[i]) ||
            std::fabs(state.values[7 + i] - expected[i]) > 1e-9 * (1 + std::fabs(expected[i])))
            return false;
    return true;
}

inline bool healthy(const State &state) {
    static constexpr unsigned baseline[AERO_PRIMARY_COUNT] = {1, 2, 11, 12, 7, 8, 9};
    if (!coherent(state))
        return false;
    for (unsigned i = 0; i < AERO_PRIMARY_COUNT; ++i)
        if (state.values[i] != state.config[baseline[i]])
            return false;
    return true;
}

inline bool same_config(const State &saved, const State &current) {
    return std::memcmp(saved.config, current.config, sizeof(saved.config)) == 0;
}

template <typename Read> bool capture(uintptr_t physics, State &state, Read read) {
    State candidate{};
    for (unsigned i = 0; i < AERO_FIELD_COUNT; ++i)
        if (!read(physics + aero_field_offsets[i], candidate.values[i]))
            return false;
    for (unsigned i = 0; i < AERO_CONFIG_COUNT; ++i)
        if (!read(physics + config_offsets[i], candidate.config[i]))
            return false;
    if (!coherent(candidate))
        return false;
    state = candidate;
    return true;
}
} // namespace sector_aero
#endif
#endif
