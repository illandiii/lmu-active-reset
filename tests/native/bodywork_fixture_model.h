#ifndef SECTOR_BODYWORK_FIXTURE_MODEL_H
#define SECTOR_BODYWORK_FIXTURE_MODEL_H
#include <array>
#include <cstring>
#include <windows.h>

struct BodyworkFixtureModel {
    alignas(8) unsigned char component[0x620]{};
    alignas(8) unsigned char part[0x28]{}, mesh[0x300]{}, submesh[0xe0]{};
    unsigned char vertices[2 * 0x58]{}, original[2 * 0x58]{};
    unsigned char *root = nullptr;
    BodyworkFixtureModel() {
        InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(component + 0x5e8));
    }
    template <typename T> static void write(unsigned char *p, T value) {
        std::memcpy(p, &value, sizeof(value));
    }
    void bind(void *graphics) {
        root = static_cast<unsigned char *>(graphics);
        write(root + 0x1a78, int32_t{1});
        write(root + 0x1a80, reinterpret_cast<uintptr_t>(component));
        std::memset(root + 0x1a88, 0, 32);
        write(component + 0xc, uint32_t{0});
        component[8] = component[0x18] = 0;
        write(component + 0x40, uint32_t{0});
        write(component + 0x1c, int32_t{1});
        write(component + 0x20, reinterpret_cast<uintptr_t>(part));
        write(part, reinterpret_cast<uintptr_t>(mesh));
        write(part + 0x10, reinterpret_cast<uintptr_t>(original));
        write(mesh + 0x14c, int32_t{2});
        write(mesh + 0x150, reinterpret_cast<uintptr_t>(vertices));
        write(mesh + 0x168, int32_t{1});
        write(mesh + 0x170, reinterpret_cast<uintptr_t>(submesh));
        write(submesh + 0xc8, uint32_t{0xa0});
        for (size_t i = 0; i < sizeof(vertices); ++i)
            vertices[i] = original[i] = static_cast<unsigned char>(i);
    }
    void damage() {
        vertices[27] ^= 0xff;
        vertices[0x58 + 27] ^= 0xff;
        write(component + 0xc, uint32_t{0x42200000});
        write(component + 0x40, uint32_t{2});
        write(submesh + 0xc8, uint32_t{0xa2});
        write(root + 0x1a88, uint32_t{2});
    }
    bool restored() const {
        return !std::memcmp(vertices, original, sizeof(vertices)) && component[0x18] == 1 &&
               component[0x40] == 0 && root[0x1a88] == 0 && submesh[0xc8] == 0xa0;
    }
};
#endif
