#include "restore_bindings_25661166.h"
#include "aero_restore_25661166.h"
#include <stdint.h>
#include <stdlib.h>
static int field(uint32_t offset, uint32_t size, uint32_t wanted, uint32_t bytes) {
    return offset == wanted && size == bytes;
}
int restore_binding_field(const RestoreBindings *b, uint32_t slot, uint32_t off, uint32_t n) {
    if (!b || b->body_count > 32 || b->constraint_count > 64 || b->engine_channels > 2 ||
        slot >= RESTORE_OBJECT_COUNT || !n || n > 128 || !b->epoch)
        return 0;
    uintptr_t address = b->address[slot];
    if (address < 0x10000 || address >= UINT64_C(0x800000000000) || off > 0x5000 - n ||
        address > UINT64_C(0x800000000000) - off - n)
        return 0;
#define F(offset, bytes) field(off, n, (offset), (bytes))
    if (slot == RESTORE_COLLISION_EXPORT_SLOT)
        return F(0, 96) || F(0x68, 4); /* Vehicle kinematics and exported mass. */
    if (slot >= RESTORE_THERMAL_BASE) {
        uint32_t pool = (slot - RESTORE_THERMAL_BASE) % 9;
        return F(0, 40) || F(0x40, 32) || (pool < 7 && (F(0x28, 16) || F(0x100, 8)));
    }
    if (slot < 32) {
        if (slot >= b->body_count)
            return 0;
        return F(8, 8) || F(0x10, 24) || F(0x28, 24) || F(0x40, 24) || F(0x58, 8) || F(0x60, 48) ||
               F(0x90, 72) || F(0xd8, 4) || F(0xe0, 24) || F(0xf8, 24) || F(0x110, 24) ||
               F(0x128, 24) || F(0x140, 24);
    }
    if (slot < 96) {
        uint32_t index = slot - 32;
        if (index >= b->constraint_count)
            return 0;
        uint32_t kind = b->constraint_kind[index], soft, stride, rows, jo, wo;
        if (kind == 1) {
            soft = 0x118;
            stride = 0;
            rows = 1;
            jo = 0xb0;
            wo = 0x140;
        } else if (kind == 2) {
            soft = 0x110;
            stride = 0xf0;
            rows = 3;
            jo = 0xa8;
            wo = 0x138;
        } else if (kind == 3) {
            soft = 0xf8;
            stride = 0xa8;
            rows = 2;
            jo = 0xc0;
            wo = 0x120;
        } else if (kind == 4) {
            soft = 0xd0;
            stride = 0xe8;
            rows = 3;
            jo = 0x98;
            wo = 0xf8;
        } else
            return 0;
        if (kind <= 3 && F(0x50, 48))
            return 1;
        for (uint32_t j = 0; j < rows; j++) {
            uint32_t d = j * stride;
            for (uint32_t k = 0; k < 6; k++)
                if (F(d + soft - 8 + k * 8, 8))
                    return 1;
            if (kind <= 2) {
                if (F(d + jo, 96) || F(d + wo, 96))
                    return 1;
            } else if (F(d + (kind == 3 ? 0xa8 : 0x60), 24) || F(d + jo, 24) ||
                       F(d + jo + 24, 24) || F(d + wo, 24) || F(d + wo + 24, 24))
                return 1;
        }
        return 0;
    }
    if (slot < 100) {
        if (F(0xc18, 8) || F(0xc20, 24) || F(0xc, 4) || F(0xfb0, 8) || F(0xfb8, 8) || F(0xfc0, 8) ||
            F(0xce4, 12))
            return 1;
        for (uint32_t s = 0; s < 8; s++)
            if (F(0x28 + s * 0x60, 8) || F(0x30 + s * 0x60, 24) || F(0x48 + s * 0x60, 24) ||
                F(0x60 + s * 0x60, 16))
                return 1;
        return 0;
    }
    if (slot < 104)
        return F(0x4b8, 8) || F(0x5d8, 8) || F(0x120, 16) || F(0x5e0, 32) || F(0x630, 8) ||
               F(0x138, 8) || F(0x302, 1) || F(0x308, 32);
    if (slot == 104) {
        for (unsigned i = 0; i < AERO_FIELD_COUNT; ++i)
            if (F(aero_field_offsets[i], 8))
                return 1;
        return F(0x448, 8) || F(0x450, 8) || F(0x18, 8) || F(0x20, 8) || F(0x30, 8) || F(0x38, 8) ||
               F(0x46c8, 8) || F(0x48a0, 8) || F(0x48a8, 8) || F(0x48b0, 8) || F(0x48b8, 8) ||
               F(0x48e8, 8) || F(0x48f0, 8) || F(0x48f8, 8) || F(0x49d8, 8) || F(0x4a18, 8) ||
               F(0x4a58, 8) || F(0x46d0, 8) || F(0x46e0, 8) || F(0x46e8, 8) || F(0x4b38, 8) ||
               F(0x4ac0, 8) || F(0x4668, 8) || F(0x50, 8) || F(0x4b30, 4) || F(0x4bb8, 4) ||
               F(0x4ba8, 16) || F(0x4ae8, 8) || F(0x48, 8) || F(0x4810, 8) || F(0x4818, 8) ||
               F(0x4820, 2) || F(0x4828, 24) || F(0x24b0, 24) || F(0x2500, 72) || F(0x4bc8, 48) ||
               F(0x2498, 24) || F(0x2550, 24);
    }
    if (slot == 105) {
        if (F(0x1f8, 8) || F(0x250, 8) || F(0x260, 8) || F(0x280, 8) || F(0x208, 56) ||
            F(0x240, 12) || F(0x50, 8) || F(0x1b0, 8))
            return 1;
        for (uint32_t i = 0; i < b->engine_channels; i++)
            if (F(0x98 + i * 0x70, 16))
                return 1;
        return 0;
    }
    if (slot == 106)
        return F(0x64, 4);
    return F(0, 8) || F(8, 8);
#undef F
}
typedef struct AddressRange {
    uintptr_t start, end;
} AddressRange;
static int compare_range(const void *a, const void *b) {
    const AddressRange *x = a;
    const AddressRange *y = b;
    return x->start < y->start ? -1 : x->start > y->start;
}
int restore_bindings_validate(const RestoreBindings *b, const RestorePlan *p) {
    if (!b || !p || b->epoch != p->epoch || !p->count || p->count > RESTORE_MAX_FIELDS)
        return RESTORE_BAD_PLAN;
    AddressRange ranges[RESTORE_MAX_FIELDS];
    for (size_t i = 0; i < p->count; i++) {
        const RestoreField *f = p->fields + i;
        if (f->reserved || !restore_binding_field(b, f->object_slot, f->field_offset, f->size))
            return RESTORE_FIELD_FORBIDDEN;
        uintptr_t start = b->address[f->object_slot] + f->field_offset;
        ranges[i] = (AddressRange){start, start + f->size};
    }
    qsort(ranges, p->count, sizeof(*ranges), compare_range);
    for (size_t i = 1; i < p->count; i++)
        if (ranges[i].start < ranges[i - 1].end)
            return RESTORE_BAD_PLAN;
    return RESTORE_OK;
}
