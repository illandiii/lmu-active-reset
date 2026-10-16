#include "../../src/native/checkpoints/checkpoint_shift_transient.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    ShiftTransientState saved = {0}, current, out, prior;
    ShiftTransientBank bank, prior_bank;
    saved.clock_a8 = 100;
    saved.event_50 = 99.75;
    saved.dt_b0 = .0025;
    saved.timer_4ac0 = -.2;
    saved.accumulators[0] = .7f;
    saved.accumulators[1] = .2f;
    saved.gears[0] = 6;
    saved.gears[1] = 0;
    saved.gears[2] = 3;
    saved.gears[3] = 2;
    saved.gears[4] = 1;
    saved.gears[5] = 1;
    saved.request[0] = 3;
    saved.request[1] = 4;
    assert(shift_transient_save(&bank, 42, &saved));
    prior_bank = bank;
    current = saved;
    current.clock_a8 = 130;
    current.event_50 = 129.9;
    assert(shift_transient_prepare(&bank, 42, &current, &out));
    assert(out.event_50 == 129.75 && out.timer_4ac0 == saved.timer_4ac0 && out.gears[2] == 3);
    prior = out;
    assert(!shift_transient_prepare(&bank, 43, &current, &out) &&
           !memcmp(&out, &prior, sizeof(out)));
    current.clock_a8 = 99;
    current.event_50 = 98;
    assert(!shift_transient_prepare(&bank, 42, &current, &out));
    current = saved;
    current.dt_b0 = .005;
    assert(!shift_transient_prepare(&bank, 42, &current, &out));
    current = saved;
    current.gears[0] = 7;
    assert(!shift_transient_prepare(&bank, 42, &current, &out));
    current = saved;
    current.gears[2] = 7;
    assert(!shift_transient_save(&bank, 43, &current) && !memcmp(&bank, &prior_bank, sizeof(bank)));
    current = saved;
    current.event_50 = 101;
    assert(!shift_transient_valid(&current));
    current = saved;
    current.timer_4ac0 = NAN;
    assert(!shift_transient_valid(&current));
    current = saved;
    current.accumulators[0] = 1.1f;
    assert(!shift_transient_valid(&current));
    current = saved;
    current.accumulators[1] = 2;
    assert(shift_transient_valid(&current));
    current = saved;
    current.gears[4] = -2;
    assert(!shift_transient_valid(&current));
    puts("PASS shift preparation: elapsed request age preserved, signed timer retained, transient "
         "gear state, sequence/config/time refusal; no live writer.");
    return 0;
}
