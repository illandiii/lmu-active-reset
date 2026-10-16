#include "../../src/native/checkpoints/checkpoint_bank.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            printf("FAIL checkpoint line %d\n", __LINE__);                                         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static PartialCheckpoint s;
static CheckpointBank bank, prior;
static CheckpointPreparation out, oldout;
int main(void) {
    s.identity = (CheckpointIdentity){1, 2, 3, 4, {1}};
    s.sequence = 7;
    s.simulation_time = 12;
    s.body_count = 2;
    s.constraint_count = 1;
    for (int i = 0; i < 2; i++) {
        s.bodies[i].mass = (RigidMassProperties){1, {1, 1, 1}};
        for (int j = 0; j < 3; j++)
            s.bodies[i].orientation[j * 4] = 1;
    }
    s.bodies[0].position_world[0] = 2;
    s.bodies[0].velocity_world[0] = 5;
    s.recipes[0].kind = CONSTRAINT_BAR;
    s.recipes[0].body_indices[1] = 1;
    s.memory[0].kind = CONSTRAINT_BAR;
    s.memory[0].body_indices[1] = 1;
    s.memory[0].row_count = 1;
    s.memory[0].rows[0] = (ConstraintRowMemory){.5, -1, 1};
    RigidMassProperties mass[2] = {s.bodies[0].mass, s.bodies[1].mass};
    CHECK(checkpoint_prepare(&bank, &s.identity, mass, 2, s.recipes, 1, &out) == CHECKPOINT_EMPTY);
    CHECK(!checkpoint_save(&bank, &s));
    prior = bank;
    s.memory[0].rows[0].impulse = NAN;
    CHECK(checkpoint_save(&bank, &s) == CHECKPOINT_INVALID_STATE &&
          !memcmp(&bank, &prior, sizeof(bank)));
    s.memory[0].rows[0].impulse = .5;
    CHECK(!checkpoint_prepare(&bank, &s.identity, mass, 2, s.recipes, 1, &out));
    CHECK(out.bodies[0].state.velocity_world[0] == 5 && out.memory[0].rows[0].impulse == .5 &&
          out.full_restore_supported == 0);
    oldout = out;
    s.identity.session_epoch++;
    CHECK(checkpoint_prepare(&bank, &s.identity, mass, 2, s.recipes, 1, &out) ==
              CHECKPOINT_IDENTITY_CHANGED &&
          !memcmp(&out, &oldout, sizeof(out)));
    s.identity.session_epoch--;
    s.identity.executable_sha256[0]++;
    CHECK(checkpoint_prepare(&bank, &s.identity, mass, 2, s.recipes, 1, &out) ==
          CHECKPOINT_IDENTITY_CHANGED);
    s.identity.executable_sha256[0]--;
    mass[1].inverse_mass = .5;
    CHECK(checkpoint_prepare(&bank, &s.identity, mass, 2, s.recipes, 1, &out) ==
              CHECKPOINT_MASS_CHANGED &&
          !memcmp(&out, &oldout, sizeof(out)));
    mass[1].inverse_mass = 1;
    s.recipes[0].body_indices[1] = 0;
    CHECK(checkpoint_prepare(&bank, &s.identity, mass, 2, s.recipes, 1, &out) ==
              CHECKPOINT_TOPOLOGY_CHANGED &&
          !memcmp(&out, &oldout, sizeof(out)));
    checkpoint_clear(&bank);
    CHECK(bank.occupied == 0);
    s.body_count = 9;
    s.recipes[0].body_indices[1] = 1;
    MechanicalLinks links = {0, {1, 2, 3, 4}, {5, 6, 7, 8}};
    MechanicalDependencies deps = {0}, current;
    deps.fuel = (FuelSnapshot){10, 0, 1, 0, 0};
    for (int i = 0; i < 9; i++) {
        s.bodies[i] = (RigidSavedBody){0};
        s.bodies[i].mass = (RigidMassProperties){1, {1, 1, 1}};
        for (int j = 0; j < 3; j++)
            s.bodies[i].orientation[j * 4] = 1;
    }
    s.bodies[0].position_world[0] = 2;
    s.bodies[0].mass.inverse_mass = .1;
    for (int w = 0; w < 4; w++) {
        deps.wheels[w].base_130 = 1;
        for (int j = 0; j < 3; j++)
            deps.tires[w].total_table[j] = 1;
    }
    CHECK(!checkpoint_save_mechanical(&bank, &s, &links, &deps));
    current = deps;
    current.fuel.quantity = 5;
    RigidMassProperties changed[9];
    for (int i = 0; i < 9; i++)
        changed[i] = s.bodies[i].mass;
    changed[0].inverse_mass = .2;
    CHECK(checkpoint_prepare(&bank, &s.identity, changed, 9, s.recipes, 1, &out) ==
          CHECKPOINT_MASS_CHANGED);
    CHECK(!checkpoint_prepare_mechanical(&bank, &s.identity, changed, 9, s.recipes, 1, &current,
                                         &out));
    CHECK(out.bodies[0].state.mass.inverse_mass == .1);
    oldout = out;
    current.wheels[3].base_130 = 2;
    CHECK(checkpoint_prepare_mechanical(&bank, &s.identity, changed, 9, s.recipes, 1, &current,
                                        &out) == CHECKPOINT_MASS_CHANGED &&
          !memcmp(&out, &oldout, sizeof(out)));
    puts("PASS RAM checkpoint: owned save, invalid replacement preserves prior slot; "
         "identity/topology/mass refusal; prepare preserves velocities and impulse memory without "
         "double application. Partial only.");
    return 0;
}
