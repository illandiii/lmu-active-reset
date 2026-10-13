#include "restore_plan_25661166.h"
#include <math.h>
#include <string.h>
static int append(RestorePlan *p, const RestoreBackend *b, uint32_t slot, uint32_t offset,
                  const void *value, uint32_t size) {
    if (p->count >= RESTORE_MAX_FIELDS || !size || size > RESTORE_FIELD_BYTES)
        return 0;
    if (!b->allowed(b->user, slot, offset, size))
        return 0;
    RestoreField *f = p->fields + p->count;
    memset(f, 0, sizeof(*f));
    f->object_slot = slot;
    f->field_offset = offset;
    f->size = size;
    if (!b->read(b->user, slot, offset, f->expected, size))
        return 0;
    memcpy(f->desired, value, size);
    ++p->count;
    return 1;
}
#define ADD(slot, offset, value)                                                                   \
    do {                                                                                           \
        if (!append(work, b, (uint32_t)(slot), (offset), &(value), (uint32_t)sizeof(value)))       \
            return RESTORE_FIELD_FORBIDDEN;                                                        \
    } while (0)
int restore_plan_25661166(const EngineBank *bank, const EnginePreparation *prepared, uint64_t epoch,
                          const RestoreBackend *b, RestorePlan *work, RestorePlan *out) {
    if (!bank || !prepared || !b || !b->context || !b->allowed || !b->read || !work || !out ||
        work == out || !epoch)
        return RESTORE_BAD_PLAN;
    const PersistentCheckpointBank *persistent = &bank->prior.state.state.vehicle.mechanical;
    const PartialCheckpoint *saved = &persistent->base.saved;
    const VehiclePartialPreparation *vehicle = &prepared->prior.state.state.vehicle;
    const CheckpointPreparation *pose = &vehicle->mechanical;
    if (persistent->base.occupied != 1 || saved->sequence != pose->sequence ||
        saved->body_count != pose->body_count ||
        saved->constraint_count != pose->constraint_count || !saved->body_count ||
        saved->body_count > 32 || saved->constraint_count > 64)
        return RESTORE_BAD_PLAN;
    if (!b->context(b->user, epoch))
        return RESTORE_CONTEXT_CHANGED;
    memset(work, 0, sizeof(*work));
    work->epoch = epoch;
    for (size_t i = 0; i < pose->body_count; i++) {
        const RigidPreparedBody *body = pose->bodies + i;
        const BodyHistory *history = prepared->prior.history.bodies + i;
        ADD(i, 0x40, body->state.position_world);
        ADD(i, 0x28, body->state.velocity_world);
        ADD(i, 0x90, body->state.orientation);
        ADD(i, 0x110, body->state.momentum_world);
        ADD(i, 0x58, body->state.mass.inverse_mass);
        ADD(i, 0xf8, body->state.mass.inverse_inertia_local);
        double mass = 1 / body->state.mass.inverse_mass, inertia[3];
        for (int j = 0; j < 3; j++)
            inertia[j] = 1 / body->state.mass.inverse_inertia_local[j];
        for (int w = 0; w < 4; w++)
            if (persistent->base.links.tires[w] == i)
                mass = prepared->prior.state.tire_mass_cache[w];
        if (!isfinite(mass) || mass <= 0 || !isfinite(inertia[0]) || !isfinite(inertia[1]) ||
            !isfinite(inertia[2]))
            return RESTORE_BAD_PLAN;
        ADD(i, 8, mass);
        ADD(i, 0x140, inertia);
        ADD(i, 0x60, body->inverse_inertia_world);
        ADD(i, 0xe0, body->angular_velocity_world);
        ADD(i, 0x10, history->force);
        ADD(i, 0x128, history->torque);
        ADD(i, 0xd8, history->renormalization);
    }
    for (size_t i = 0; i < pose->constraint_count; i++) {
        const ConstraintRecipe *recipe = saved->recipes + i;
        const ConstraintMemory *memory = pose->memory + i;
        uint32_t slot = RESTORE_CONSTRAINT_BASE + (uint32_t)i, soft, stride, jo, wo;
        if (recipe->kind == 1) {
            soft = 0x118;
            stride = 0;
            jo = 0xb0;
            wo = 0x140;
        } else if (recipe->kind == 2) {
            soft = 0x110;
            stride = 0xf0;
            jo = 0xa8;
            wo = 0x138;
        } else if (recipe->kind == 3) {
            soft = 0xf8;
            stride = 0xa8;
            jo = 0xc0;
            wo = 0x120;
        } else if (recipe->kind == 4) {
            soft = 0xd0;
            stride = 0xe8;
            jo = 0x98;
            wo = 0xf8;
        } else
            return RESTORE_BAD_PLAN;
        size_t expected = recipe->kind == 1 ? 1 : recipe->kind == 3 ? 2 : 3;
        if (memory->row_count != expected)
            return RESTORE_BAD_PLAN;
        if (recipe->kind == 1)
            ADD(slot, 0x50, recipe->geometry.bar.anchors_local);
        if (recipe->kind == 2)
            ADD(slot, 0x50, recipe->geometry.ball.anchors_local);
        for (size_t j = 0; j < expected; j++) {
            uint32_t delta = (uint32_t)j * stride;
            const SolverRowHistory *row = &prepared->prior.history.rows[i][j];
            double softness = recipe->kind == 1   ? recipe->geometry.bar.softness
                              : recipe->kind == 2 ? recipe->geometry.ball.softness[j]
                                                  : recipe->geometry.angular.softness[j];
            ADD(slot, delta + soft, softness);
            ADD(slot, delta + soft - 8, row->bias);
            ADD(slot, delta + soft + 8, memory->rows[j].impulse);
            ADD(slot, delta + soft + 16, memory->rows[j].lower);
            ADD(slot, delta + soft + 24, memory->rows[j].upper);
            ADD(slot, delta + soft + 32, row->coefficient);
            if (recipe->kind <= 2) {
                ADD(slot, delta + jo, row->jacobian);
                ADD(slot, delta + wo, row->weighted);
            } else {
                ADD(slot, delta + (recipe->kind == 3 ? 0xa8 : 0x60),
                    recipe->geometry.angular.axes_local[j]);
                if (!append(work, b, slot, delta + jo, row->jacobian + 3, 24) ||
                    !append(work, b, slot, delta + jo + 24, row->jacobian + 9, 24) ||
                    !append(work, b, slot, delta + wo, row->weighted + 3, 24) ||
                    !append(work, b, slot, delta + wo + 24, row->weighted + 9, 24))
                    return RESTORE_FIELD_FORBIDDEN;
            }
        }
    }
    const MechanicalDependencies *d = &vehicle->dependencies;
    for (int w = 0; w < 4; w++) {
        const TirePersistentState *t = vehicle->tires + w;
        uint32_t slot = RESTORE_TIRE_BASE + w;
        ADD(slot, 0xc18, t->total_mass);
        ADD(slot, 0xc20, t->cached_sector_inertia);
        ADD(slot, 0xc, t->update_phase);
        ADD(slot, 0xfb0, t->thermal_fb0);
        ADD(slot, 0xfb8, t->thermal_fb8);
        ADD(slot, 0xfc0, t->accumulated_fc0);
        ADD(slot, 0xce4, d->tires[w].total_table);
        for (int i = 0; i < 8; i++) {
            ADD(slot, 0x28 + i * 0x60, t->sector_mass[i]);
            ADD(slot, 0x48 + i * 0x60, d->tires[w].sectors[i]);
        }
        ADD(RESTORE_WHEEL_BASE + w, 0x4b8, d->wheels[w].term_4b8);
        ADD(RESTORE_WHEEL_BASE + w, 0x5d8, d->wheels[w].term_5d8);
    }
    ADD(RESTORE_VEHICLE, 0x448, d->fuel.quantity);
    ADD(RESTORE_VEHICLE, 0x450, d->fuel.quantity_rate);
    const PowertrainPartialState *power = &vehicle->powertrain;
    ADD(RESTORE_ENGINE, 0x1f8, power->engine_1f8);
    ADD(RESTORE_ENGINE, 0x250, power->engine_250);
    ADD(RESTORE_ENGINE, 0x260, power->engine_260);
    ADD(RESTORE_ENGINE, 0x280, power->engine_280);
    ADD(RESTORE_VEHICLE, 0x48a0, power->hdv_48a0);
    ADD(RESTORE_VEHICLE, 0x48a8, power->hdv_48a8);
    ADD(RESTORE_VEHICLE, 0x48b0, power->hdv_48b0);
    ADD(RESTORE_VEHICLE, 0x48b8, power->hdv_48b8);
    ADD(RESTORE_VEHICLE, 0x48e8, power->hdv_48e8);
    ADD(RESTORE_VEHICLE, 0x48f0, power->hdv_48f0);
    ADD(RESTORE_VEHICLE, 0x48f8, power->hdv_48f8);
    const TransmissionPartialState *tr = &prepared->prior.state.state.transmission;
    ADD(RESTORE_SHAFT, 0, tr->shaft_0);
    ADD(RESTORE_SHAFT, 8, tr->shaft_8);
    ADD(RESTORE_GEARBOX, 0x64, tr->gear_64);
    ADD(RESTORE_VEHICLE, 0x18, tr->input_18);
    ADD(RESTORE_VEHICLE, 0x20, tr->input_20);
    ADD(RESTORE_VEHICLE, 0x30, tr->input_30);
    ADD(RESTORE_VEHICLE, 0x38, tr->input_38);
    ADD(RESTORE_VEHICLE, 0x46c8, prepared->prior.history.driveline_timer_46c8);
    ADD(RESTORE_ENGINE, 0x208, prepared->integration.outputs_208);
    ADD(RESTORE_ENGINE, 0x240, prepared->integration.fields_240);
    if (prepared->integration.channel_count > 2)
        return RESTORE_BAD_PLAN;
    for (uint32_t i = 0; i < prepared->integration.channel_count; i++)
        ADD(RESTORE_ENGINE, 0x98 + i * 0x70, prepared->integration.channel_state[i]);
    if (!b->context(b->user, epoch))
        return RESTORE_CONTEXT_CHANGED;
    *out = *work;
    return RESTORE_OK;
}
