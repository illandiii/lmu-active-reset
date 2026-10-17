// Read-only typed layout extracted from the validated v31 observer.
#include "vehicle_state_reader.h"
#include <cmath>
#include <cstring>

namespace {
template <class T> static bool read_field(uintptr_t p, T &out) {
    SIZE_T got = 0;
    return p >= 0x10000 && p < 0x800000000000ULL &&
           ReadProcessMemory(GetCurrentProcess(), (void *)p, &out, sizeof(out), &got) &&
           got == sizeof(out);
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

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}
struct Reader {
    SnapshotFrame *diagnostic;
    WheelDeformation deformation[4];
    bool fail(unsigned line) {
        diagnostic->binding_status = line;
        return false;
    }
    uintptr_t module_base;
    uintptr_t native_bindings[RESTORE_OBJECT_COUNT];
    uintptr_t player() {

        uintptr_t owner = 0, p = 0;
        return read_field(module_base + 0x1e194c8, owner) && read_field(owner + 0x159a8, p) ? p : 0;
    }
    bool mechanical(uintptr_t p, const uintptr_t *bodies, SnapshotFrame &f) {
        auto &d = f.dependencies;
        auto &links = f.links;
        uintptr_t fuel = p + 0x448, fuel_body = 0, branch = 0;
        if (!read_field(fuel, d.fuel.quantity) || !read_field(fuel + 8, d.fuel.quantity_rate) ||
            !read_field(fuel + 0x60, d.fuel.density) ||
            !read_field(fuel + 0x80, d.fuel.base_mass) || !read_field(fuel + 0x78, fuel_body) ||
            !read_field(p + 0x550, branch))
            return fail(__LINE__);
        size_t matches = 0;
        for (size_t i = 0; i < f.root_count; i++)
            if (bodies[i] == fuel_body) {
                links.fuel = i;
                ++matches;
            }
        if (matches != 1)
            return fail(__LINE__);
        if (!branch) {
            uintptr_t game = 0;
            float initial = 0;
            unsigned char indices[14];
            double term = 0, tail = 0, extra = 0;
            if (!read_field(p + 0x4bc0, game) || !read_field(game + 0x440d8, initial) ||
                !read_field(p + 0x138, indices))
                return fail(__LINE__);
            d.fuel.additional_mass = initial;
            for (int i = 0; i < 14; i++)
                if (indices[i] > 3)
                    return fail(__LINE__);
            for (int i = 0; i < 13; i++) {
                if (!read_field(p + 0x148 + i * 0x20 + indices[i] * 8, term))
                    return fail(__LINE__);
                d.fuel.additional_mass += term;
            }
            if (!read_field(p + 0x2e8 + indices[13] * 8, tail) || !read_field(p + 0x558, extra))
                return fail(__LINE__);
            tail += extra;
            if (!read_field(p + 0x2608, term))
                return fail(__LINE__);
            d.fuel.additional_mass += term;
            d.fuel.additional_mass += tail;
        }
        for (int w = 0; w < 4; w++) {
            uintptr_t wheel = p + 0x618 + w * 0x6b0, body = 0, tire = 0;
            if (!read_field(wheel + 0x1d0, body) || !read_field(wheel + 0x3e8, tire) ||
                tire + 0xac0 != bodies[f.root_count + w])
                return fail(__LINE__);
            matches = 0;
            for (size_t i = 0; i < f.root_count; i++)
                if (bodies[i] == body) {
                    links.wheels[w] = i;
                    ++matches;
                }
            if (matches != 1)
                return fail(__LINE__);
            links.tires[w] = f.root_count + w;
            uintptr_t hub = 0, rear = 0;
            unsigned char flags[2];
            auto &phase = f.wheel_phase[w];
            if (!read_field(p + 0x20d8, rear) || !read_field(wheel + 0x301, flags) ||
                !read_field(wheel + 0x308, deformation[w]))
                return fail(__LINE__);
            // A bent, attached wheel has a different spin axis, not a new identity.
            // Detachment also changes the active constraint set and is not admitted.
            if (flags[0])
                return fail(91001 + w);
            if (flags[1] > 1)
                return fail(91010 + w);
            if (w >= 2 && rear)
                hub = rear;
            else if (!read_field(wheel + 0x1d8, hub))
                return fail(__LINE__);
            size_t hub_matches = 0;
            for (size_t i = 0; i < f.root_count; i++)
                if (bodies[i] == hub) {
                    phase.hub_index = (uint32_t)i;
                    ++hub_matches;
                }
            if (hub_matches != 1)
                return fail(__LINE__);
            phase.wheel_index = (uint32_t)links.wheels[w];
            phase.spin.attached = 1;
            phase.spin.deformed = flags[1];
            if (!read_field(wheel + 0x120, phase.spin.angle) ||
                !read_field(wheel + 0x128, phase.spin.angular_rate))
                return fail(__LINE__);
            f.topology_token = hash_bytes(f.topology_token, &phase.hub_index, sizeof(uint32_t) * 2);
            if (!read_field(wheel + 0x130, d.wheels[w].base_130) ||
                !read_field(wheel + 0x4b8, d.wheels[w].term_4b8) ||
                !read_field(wheel + 0x5d8, d.wheels[w].term_5d8) ||
                !read_field(wheel + 0x4f0, d.wheels[w].factor_4f0) ||
                !read_field(tire + 0xce4, d.tires[w].total_table))
                return fail(__LINE__);
            for (int sector = 0; sector < 8; sector++)
                if (!read_field(tire + 0x48 + sector * 0x60, d.tires[w].sectors[sector]))
                    return fail(__LINE__);
            for (int i = 0; i < 8; i++) {
                auto &cache = f.native_cache.sectors[w][i];
                if (!read_field(tire + 0x30 + i * 0x60, cache.first_moment) ||
                    !read_field(tire + 0x60 + i * 0x60, cache.aggregate))
                    return fail(__LINE__);
            }
            uintptr_t pool = 0, inner = 0, outer = 0, table = 0;
            if (!read_field(tire + 0x318, pool) || !read_field(tire + 0xa10, inner) ||
                !read_field(tire + 0xa18, outer) || !read_field(tire + 0xa28, table))
                return fail(__LINE__);
            uintptr_t refs[4] = {pool, inner, outer, table};
            for (auto ref : refs)
                if (ref < 0x10000 || ref >= 0x800000000000ULL)
                    return fail(__LINE__);
            f.topology_token = hash_bytes(f.topology_token, refs, sizeof(refs));
            auto &grid = f.thermal_grid.tires[w];
            for (int sector = 0; sector < 8; sector++) {
                uint16_t indices[9];
                if (!read_field(tire + 0x10 + sector * 0x60, indices))
                    return fail(__LINE__);
                f.topology_token = hash_bytes(f.topology_token, indices, sizeof(indices));
                for (int j = 0; j < 9; j++) {
                    uint32_t idx = indices[j];
                    int row = sector * 9 + j;
                    if (idx >= 256)
                        return fail(__LINE__);
                    grid.angular_index[row] = idx;
                    for (int previous = 0; previous < row; previous++)
                        if (grid.angular_index[previous] == idx)
                            return fail(__LINE__);
                    uintptr_t pointers[9], expected[9];
                    if (!read_field(table + idx * 72, pointers))
                        return fail(__LINE__);
                    for (int k = 0; k < 7; k++)
                        expected[k] = pool + idx * 0x770 + k * 0x110;
                    expected[7] = inner + idx * 0x60;
                    expected[8] = outer + idx * 0x60;
                    for (int k = 0; k < 9; k++) {
                        size_t matches = 0;
                        for (auto pointer : pointers)
                            if (pointer == expected[k])
                                ++matches;
                        if (matches != 1)
                            return fail(__LINE__);
                        if (k < 7) {
                            auto &h = f.tread_history.nodes[w][row][k];
                            if (!read_field(expected[k] + 0x28, h.fields_28) ||
                                !read_field(expected[k] + 0x100, h.field_100))
                                return fail(__LINE__);
                        }
                        native_bindings[RESTORE_THERMAL_BASE + (w * 72 + row) * 9 + k] =
                            expected[k];
                        auto &cell = grid.cells[row][k];
                        if (!read_field(expected[k], cell.layers) ||
                            !read_field(expected[k] + 0x40, cell.coefficients))
                            return fail(__LINE__);
                    }
                }
            }
            auto &brake = f.brake_state[w];
            if (!read_field(wheel + 0x5d8, brake.fields_5d8) ||
                !read_field(wheel + 0x630, brake.field_630) ||
                !read_field(wheel + 0x138, brake.cache_138))
                return fail(__LINE__);
            double inverse = f.bodies[links.wheels[w]].mass.inverse_inertia_local[0];
            if (brake.fields_5d8[0] != d.wheels[w].term_5d8 ||
                std::fabs(brake.cache_138 - inverse) >
                    32 * 2.2204460492503131e-16 * std::fabs(inverse))
                return fail(__LINE__);
            native_bindings[96 + w] = tire;
            native_bindings[100 + w] = wheel;
            auto &state = f.tire_state[w];
            if (!read_field(tire + 0xac8, f.tire_mass_cache[w]))
                return fail(__LINE__);
            if (!read_field(tire + 0xc18, state.total_mass) ||
                !read_field(tire + 0xc20, state.cached_sector_inertia) ||
                !read_field(tire + 0xc, state.update_phase) ||
                !read_field(tire + 0xfb0, state.thermal_fb0) ||
                !read_field(tire + 0xfb8, state.thermal_fb8) ||
                !read_field(tire + 0xfc0, state.accumulated_fc0))
                return fail(__LINE__);
            for (int sector = 0; sector < 8; sector++)
                if (!read_field(tire + 0x28 + sector * 0x60, state.sector_mass[sector]))
                    return fail(__LINE__);
        }
        // Native tire bodies use applied persistent caches. Sector values may
        // already belong to the next update; recomputing their sum here rejects
        // a valid phase of the simulation. Share the checkpoint bank's model.
        f.mechanical_status = checkpoint_validate_cached_mechanical(
            f.bodies, f.total_count, &links, &d, f.tire_state, f.tire_mass_cache);
        return true;
    }

    bool capture_control(uintptr_t p, PowertrainControl &c) {
        uintptr_t engine = 0;
        if (!read_field(p + 0x46f0, engine))
            return fail(__LINE__);
        if (!read_field(p + 0x4810, c.ramp_d0) || !read_field(p + 0x4818, c.index_d8) ||
            !read_field(p + 0x481c, c.index_dc) || !read_field(p + 0x4820, c.active_e0) ||
            !read_field(p + 0x4821, c.latched_e1) || !read_field(p + 0x4828, c.timer_e8) ||
            !read_field(p + 0x4830, c.factor_f0) || !read_field(p + 0x4838, c.factor_f8) ||
            !read_field(engine + 0x50, c.engine_50) || !read_field(engine + 0x1b0, c.engine_1b0))
            return fail(__LINE__);
        uint64_t hash = 14695981039346656037ULL;
        for (unsigned i = 0; i < 4; i++) {
            uintptr_t v[3];
            if (!read_field(p + 0x4740 + 24 * i, v) || v[1] < v[0] || v[2] < v[1] ||
                (v[1] - v[0]) % 8 || (v[2] - v[0]) % 8 || (v[1] - v[0]) / 8 > 64)
                return fail(__LINE__);
            c.table_counts[i] = (uint32_t)((v[1] - v[0]) / 8);
            hash = hash_bytes(hash, &c.table_counts[i], 4);
            for (unsigned j = 0; j < c.table_counts[i]; j++) {
                double value;
                if (!read_field(v[0] + 8 * j, value) || !std::isfinite(value))
                    return fail(__LINE__);
                hash = hash_bytes(hash, &value, 8);
            }
        }
        c.table_token = hash ? hash : 1;
        return powertrain_control_valid(&c);
    }
    bool capture(uintptr_t p, SnapshotFrame &f) {
        if (!capture_control(p, f.powertrain_control))
            return fail(__LINE__);
        auto &shift = f.shift_observed;
        if (!read_field(p + 0x4ac0, shift.timer_4ac0) ||
            !read_field(p + 0x4668, shift.control_4668) || !read_field(p + 0xa8, shift.clock_a8) ||
            !read_field(p + 0x50, shift.event_50) || !read_field(p + 0xb0, shift.dt_b0) ||
            !read_field(p + 0x4b30, shift.accumulators[0]) ||
            !read_field(p + 0x4bb8, shift.accumulators[1]) ||
            !read_field(p + 0x4ba0, shift.gears) || !read_field(p + 0x4ae8, shift.notification) ||
            !read_field(p + 0x48, shift.request))
            return fail(__LINE__);
        if (!read_field(p + 0x4900, f.driveline_cache.drive_type) ||
            !read_field(p + 0x49d8, f.driveline_cache.torque[0]) ||
            !read_field(p + 0x4a18, f.driveline_cache.torque[1]) ||
            !read_field(p + 0x4a58, f.driveline_cache.torque[2]) ||
            !driveline_cache_valid(&f.driveline_cache))
            return fail(__LINE__);
        if (!read_field(p + 0x46d0, f.driveline_outputs.hdv_46d0) ||
            !read_field(p + 0x46e0, f.driveline_outputs.hdv_46e0) ||
            !read_field(p + 0x46e8, f.driveline_outputs.hdv_46e8) ||
            !read_field(p + 0x4b38, f.driveline_outputs.hdv_4b38) ||
            !driveline_outputs_valid(&f.driveline_outputs))
            return fail(__LINE__);
        uintptr_t game_vehicle = 0;
        if (!read_field(p + 0x4bc0, game_vehicle) ||
            !read_field(game_vehicle + 0x1cf9c, f.terminal_failure))
            return fail(__LINE__);
        uintptr_t bodies[64], constraints[64];
        size_t roots = 0, nc = 0;
        if (player() != p || !vector(p + 0x23f8, bodies, roots) || roots > 28 ||
            !vector(p + 0x2410, constraints, nc))
            return fail(__LINE__);
        for (int w = 0; w < 4; w++) {
            uintptr_t tire = 0;
            if (!read_field(p + 0x618 + w * 0x6b0 + 0x3e8, tire) || tire < 0x10000)
                return fail(__LINE__);
            bodies[roots + w] = tire + 0xac0;
        }
        f.root_count = roots;
        f.total_count = roots + 4;
        f.constraint_count = nc;
        uint64_t token = hash_bytes(14695981039346656037ULL, &p, sizeof(p));
        token = hash_bytes(token, bodies, (roots + 4) * sizeof(uintptr_t));
        token = hash_bytes(token, constraints, nc * sizeof(uintptr_t));
        token =
            hash_bytes(token, &f.driveline_cache.drive_type, sizeof(f.driveline_cache.drive_type));
        f.topology_token = token ? token : 1;
        RigidMassProperties mass[32];
        RigidPreparedBody prepared[32];
        for (size_t i = 0; i < f.total_count; i++) {
            for (size_t j = 0; j < i; j++)
                if (bodies[j] == bodies[i])
                    return fail(__LINE__);
            native_bindings[i] = bodies[i];
            if (!body(bodies[i], f.bodies[i]))
                return fail(__LINE__);
            mass[i] = f.bodies[i].mass;
            auto &cache = f.native_cache.bodies[i];
            if (!read_field(bodies[i] + 8, cache.mass) ||
                !read_field(bodies[i] + 0x140, cache.inertia_local) ||
                !read_field(bodies[i] + 0x60, cache.inverse_world) ||
                !read_field(bodies[i] + 0xe0, cache.omega))
                return fail(__LINE__);
            auto &h = f.history.bodies[i];
            if (!read_field(bodies[i] + 0x10, h.force) ||
                !read_field(bodies[i] + 0x128, h.torque) ||
                !read_field(bodies[i] + 0xd8, h.renormalization))
                return fail(__LINE__);
            for (int j = 0; j < 3; j++)
                if (!std::isfinite(h.force[j]) || !std::isfinite(h.torque[j]))
                    return fail(__LINE__);
        }
        if (rigid_prepare_restore(f.bodies, mass, f.total_count, prepared))
            return fail(__LINE__);
        for (size_t i = 0; i < f.total_count; i++) {
            double iw[6], omega[3];
            if (!read_field(bodies[i] + 0x60, iw) || !read_field(bodies[i] + 0xe0, omega))
                return fail(__LINE__);
            for (int j = 0; j < 6; j++) {
                if (!std::isfinite(iw[j]))
                    return fail(__LINE__);
                double e = std::fabs(iw[j] - prepared[i].inverse_inertia_world[j]);
                if (e > f.inertia_error)
                    f.inertia_error = e;
            }
            for (int j = 0; j < 3; j++) {
                if (!std::isfinite(omega[j]))
                    return fail(__LINE__);
                double e = std::fabs(omega[j] - prepared[i].angular_velocity_world[j]);
                if (e > f.omega_error)
                    f.omega_error = e;
            }
        }
        for (size_t i = 0; i < nc; i++) {
            native_bindings[32 + i] = constraints[i];
            uintptr_t c = constraints[i], vt = 0, pair[2];
            auto &r = f.recipes[i];
            auto &m = f.memory[i];
            for (size_t j = 0; j < i; j++)
                if (constraints[j] == c)
                    return fail(__LINE__);
            if (!read_field(c, vt))
                return fail(__LINE__);
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
                return fail(__LINE__);
            if (!read_field(c + link, pair))
                return fail(__LINE__);
            for (int side = 0; side < 2; side++) {
                size_t matches = 0;
                for (size_t j = 0; j < roots; j++)
                    if (pair[side] == bodies[j]) {
                        r.body_indices[side] = m.body_indices[side] = j;
                        ++matches;
                    }
                if (matches != 1)
                    return fail(__LINE__);
            }
            if (pair[0] == pair[1])
                return fail(__LINE__);
            r.identity = m.identity = i;
            m.kind = r.kind;
            m.row_count = rows;
            if (r.kind == CONSTRAINT_BAR) {
                if (!read_field(c + 0x50, r.geometry.bar.anchors_local) ||
                    !read_field(c + soft, r.geometry.bar.softness))
                    return fail(__LINE__);
            } else if (r.kind == CONSTRAINT_BALL) {
                if (!read_field(c + 0x50, r.geometry.ball.anchors_local))
                    return fail(__LINE__);
            }
            for (size_t j = 0; j < rows; j++) {
                auto &row = m.rows[j];
                uintptr_t q = c + j * stride;
                auto &history = f.history.rows[i][j];
                if (!read_field(q + soft - 8, history.bias) ||
                    !read_field(q + upper + 8, history.coefficient))
                    return fail(__LINE__);
                if (r.kind <= CONSTRAINT_BALL) {
                    size_t jo = r.kind == CONSTRAINT_BAR ? 0xb0 : 0xa8,
                           wo = r.kind == CONSTRAINT_BAR ? 0x140 : 0x138;
                    if (!read_field(q + jo, history.jacobian) ||
                        !read_field(q + wo, history.weighted))
                        return fail(__LINE__);
                } else {
                    size_t jo = r.kind == CONSTRAINT_HINGE ? 0xc0 : 0x98,
                           wo = r.kind == CONSTRAINT_HINGE ? 0x120 : 0xf8;
                    double a[3], b[3], wa[3], wb[3];
                    if (!read_field(q + jo, a) || !read_field(q + jo + 24, b) ||
                        !read_field(q + wo, wa) || !read_field(q + wo + 24, wb))
                        return fail(__LINE__);
                    for (int k = 0; k < 3; k++) {
                        history.jacobian[3 + k] = a[k];
                        history.jacobian[9 + k] = b[k];
                        history.weighted[3 + k] = wa[k];
                        history.weighted[9 + k] = wb[k];
                    }
                }
                if (!std::isfinite(history.bias) || !std::isfinite(history.coefficient))
                    return fail(__LINE__);
                for (int k = 0; k < 12; k++)
                    if (!std::isfinite(history.jacobian[k]) || !std::isfinite(history.weighted[k]))
                        return fail(__LINE__);

                if (r.kind == CONSTRAINT_BALL && !read_field(q + soft, r.geometry.ball.softness[j]))
                    return fail(__LINE__);
                if (r.kind >= CONSTRAINT_HINGE &&
                    (!read_field(q + axis, r.geometry.angular.axes_local[j]) ||
                     !read_field(q + soft, r.geometry.angular.softness[j])))
                    return fail(__LINE__);
                if (!read_field(q + impulse, row.impulse) || !read_field(q + lower, row.lower) ||
                    !read_field(q + upper, row.upper))
                    return fail(__LINE__);
                if (!std::isfinite(row.impulse) || !std::isfinite(row.lower) ||
                    !std::isfinite(row.upper) || row.lower > row.upper)
                    return fail(__LINE__);
            }
        }
        ConstraintPrepared geometry[64];
        f.geometry_status =
            constraint_prepare_group(f.bodies, f.total_count, f.recipes, nc, geometry);
        f.memory_valid = 1;
        uintptr_t engine = 0, vt = 0;
        auto &aux = f.powertrain;
        if (!read_field(p + 0x46f0, engine) || !read_field(engine, vt) ||
            vt != module_base + 0x1793578)
            return fail(__LINE__);
        aux.engine_type_rva = 0x1793578;
        auto &eg = f.integration;
        if (!read_field(engine + 0x88, eg.channel_count) || eg.channel_count > 2 ||
            !read_field(engine + 0x208, eg.outputs_208) ||
            !read_field(engine + 0x240, eg.fields_240))
            return fail(__LINE__);
        for (uint32_t i = 0; i < eg.channel_count; i++)
            if (!read_field(engine + 0x98 + i * 0x70, eg.channel_state[i]))
                return fail(__LINE__);
        for (int i = 0; i < 7; i++)
            if (!std::isfinite(eg.outputs_208[i]))
                return fail(__LINE__);
        for (int i = 0; i < 3; i++)
            if (!std::isfinite(eg.fields_240[i]))
                return fail(__LINE__);
        for (uint32_t i = 0; i < eg.channel_count; i++)
            for (int j = 0; j < 2; j++)
                if (!std::isfinite(eg.channel_state[i][j]))
                    return fail(__LINE__);

        if (!read_field(engine + 0x1f8, aux.engine_1f8) ||
            !read_field(engine + 0x250, aux.engine_250) ||
            !read_field(engine + 0x260, aux.engine_260) ||
            !read_field(engine + 0x280, aux.engine_280))
            return fail(__LINE__);
        const size_t offsets[7] = {0x48a0, 0x48a8, 0x48b0, 0x48b8, 0x48e8, 0x48f0, 0x48f8};
        double *fields[7] = {&aux.hdv_48a0, &aux.hdv_48a8, &aux.hdv_48b0, &aux.hdv_48b8,
                             &aux.hdv_48e8, &aux.hdv_48f0, &aux.hdv_48f8};
        for (int i = 0; i < 7; i++)
            if (!read_field(p + offsets[i], *fields[i]) || !std::isfinite(*fields[i]))
                return fail(__LINE__);
        if (!std::isfinite(aux.engine_1f8) || !std::isfinite(aux.engine_250) ||
            !std::isfinite(aux.engine_260) || !std::isfinite(aux.engine_280))
            return fail(__LINE__);
        uintptr_t shaft = 0, gearbox = 0;
        auto &tr = f.transmission;
        if (!read_field(p + 0x4918, shaft) || !read_field(p + 0x4920, gearbox) ||
            !read_field(shaft, tr.shaft_0) || !read_field(shaft + 8, tr.shaft_8) ||
            !read_field(gearbox + 0x64, tr.gear_64) || !read_field(p + 0x18, tr.input_18) ||
            !read_field(p + 0x20, tr.input_20) || !read_field(p + 0x30, tr.input_30) ||
            !read_field(p + 0x38, tr.input_38))
            return fail(__LINE__);
        if (!std::isfinite(tr.shaft_0) || !std::isfinite(tr.shaft_8) ||
            !std::isfinite(tr.input_18) || !std::isfinite(tr.input_20) ||
            !std::isfinite(tr.input_30) || !std::isfinite(tr.input_38))
            return fail(__LINE__);
        native_bindings[104] = p;
        native_bindings[105] = engine;
        native_bindings[106] = gearbox;
        native_bindings[107] = shaft;
        if (!read_field(p + 0x46c8, f.history.driveline_timer_46c8) ||
            !std::isfinite(f.history.driveline_timer_46c8))
            return fail(__LINE__);
        if (!mechanical(p, bodies, f))
            return false;
        if (!native_cache_valid(&f.native_cache, f.bodies, f.total_count))
            return fail(__LINE__);
        if (!thermal_grid_valid(&f.thermal_grid))
            return fail(__LINE__);
        if (!tread_history_valid(&f.tread_history))
            return fail(__LINE__);
        if (!wheel_phase_deformed_valid(f.wheel_phase, deformation, f.bodies, f.total_count))
            return fail(__LINE__);
        if (!brake_state_valid(f.brake_state))
            return fail(__LINE__);
        if (player() != p)
            return fail(__LINE__);
        return true;
    }
};
} // namespace

bool vehicle_read_25661166(uintptr_t module_base, uintptr_t physics, uint64_t epoch,
                           double simulation_time, SnapshotFrame *frame,
                           RestoreBindings *bindings) {
    if (!frame || !bindings)
        return false;
    *bindings = {};
    *frame = {};
    if (!module_base || !physics || !epoch || !std::isfinite(simulation_time))
        return false;
    Reader reader{};
    reader.module_base = module_base;
    reader.diagnostic = frame;
    frame->simulation_time = simulation_time;
    if (!reader.capture(physics, *frame)) {
        if (!frame->binding_status)
            frame->binding_status = 90000;
        return false;
    }
    if (frame->geometry_status || frame->mechanical_status || !frame->memory_valid) {
        frame->binding_status = frame->geometry_status     ? 90100 + frame->geometry_status
                                : frame->mechanical_status ? 90200 + frame->mechanical_status
                                                           : 90003;
        return false;
    }
    bindings->epoch = epoch;
    std::memcpy(bindings->address, reader.native_bindings, sizeof(bindings->address));
    bindings->body_count = static_cast<uint32_t>(frame->total_count);
    bindings->constraint_count = static_cast<uint32_t>(frame->constraint_count);
    bindings->engine_channels = frame->integration.channel_count;
    for (size_t i = 0; i < frame->constraint_count; ++i)
        bindings->constraint_kind[i] = frame->recipes[i].kind;
    return true;
}

uintptr_t vehicle_scoring_25661166(uintptr_t module_base, uintptr_t physics_pointer) {
    uintptr_t native_bindings[RESTORE_OBJECT_COUNT]{};
    native_bindings[104] = physics_pointer;

    uintptr_t owner = 0, vehicle = 0, back = 0, physics = 0, physical_vehicle = 0;
    if (!read_field(module_base + 0x1e194c8, owner) || !read_field(owner, vehicle) ||
        !read_field(owner + 0x159a8, physics) || physics != native_bindings[104] ||
        !read_field(physics + 0x4bc0, physical_vehicle) || physical_vehicle != vehicle)
        return 0;
    uintptr_t score = vehicle + 0x1cec8;
    return read_field(score, back) && back == owner ? score : 0;
}
