// Owned-memory contact model, shared layout with contact_batch_fixture.cpp.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "contact_batch_25661166.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <initializer_list>
static unsigned char tire[4][0x1200], pool[4][4][0x110], surface[4][2][0x58];
static uintptr_t active[4][8];
static uint32_t bits[4];
static uint64_t calls;
static int stationary = 1, abi_errors = 0;
template <class T> static void put(void *p, const T &value) {
    memcpy(p, &value, sizeof(value));
}
template <class T> static T get(void *p) {
    T value;
    memcpy(&value, p, sizeof(value));
    return value;
}
extern "C" __declspec(dllexport) int WINAPI FixtureWheel(void *p, uintptr_t *player) {
    *player = (uintptr_t)tire;
    for (int i = 0; i < 4; i++)
        if (p == tire[i])
            return i;
    return -1;
}
extern "C" __declspec(dllexport) int WINAPI FixtureStationary() {
    return stationary;
}
extern "C" __declspec(dllexport) __declspec(noinline) void WINAPI FixtureRefresh(void *p, double dt,
                                                                                 double horizon,
                                                                                 void *world,
                                                                                 double angular) {
    ++calls;
    if (dt != .001 || horizon != .003 || world != (void *)0x12345678 || angular != 2.5) {
        ++abi_errors;
        return;
    }
    uintptr_t player;
    int w = FixtureWheel(p, &player);
    if (w < 0)
        return;
    size_t count = (get<uintptr_t>(tire[w] + 0x9f8) - (uintptr_t)active[w]) / 8;
    for (size_t i = 0; i < count;) {
        auto *n = (unsigned char *)active[w][i];
        double d = get<double>(n + 0xd0);
        if (d <= 0) {
            n[0x60] = n[0x67] = 0;
            active[w][i] = active[w][--count];
        } else
            ++i;
    }
    for (int i = 0; i < 3; i++) {
        auto *n = pool[w][i];
        if (!n[0x60]) {
            n[0x60] = 1;
            active[w][count++] = (uintptr_t)n;
        }
        double geometry[5] = {.02 + i * .001, 0, 1, 0, 3};
        put(n + 0xd0, geometry);
    }
    put(tire[w] + 0x9f8, (uintptr_t)active[w] + count * 8);
}
static void init() {
    for (int w = 0; w < 4; w++) {
        put(tire[w] + 0x318, (uintptr_t)pool[w]);
        uintptr_t v[3] = {(uintptr_t)active[w], (uintptr_t)active[w] + 24,
                          (uintptr_t)active[w] + 64};
        put(tire[w] + 0x9f0, v);
        uintptr_t sv[3] = {(uintptr_t)surface[w], (uintptr_t)surface[w] + 0xb0,
                           (uintptr_t)surface[w] + 0xb0};
        put(tire[w] + 0xa60, sv);
        uintptr_t bv[3] = {(uintptr_t)&bits[w], (uintptr_t)&bits[w] + 4, (uintptr_t)&bits[w] + 4};
        put(tire[w] + 0xa78, bv);
        bits[w] = 3;
        for (int i = 0; i < 2; i++)
            put(surface[w][i], (uintptr_t)(0x20000 + i * 0x1000));
        for (int i = 0; i < 3; i++) {
            active[w][i] = (uintptr_t)pool[w][i];
            pool[w][i][0x60] = pool[w][i][0x67] = 1;
            put(pool[w][i] + 0x62, (uint16_t)(i % 2));
            put(pool[w][i] + 0xf8, (uintptr_t)(0x20000 + (i % 2) * 0x1000));
            double geo[5] = {.02, 0, 1, 0, 3};
            put(pool[w][i] + 0xd0, geo);
        }
    }
}
