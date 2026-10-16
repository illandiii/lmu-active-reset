/* Tests against the observed branches in Score update and lap transition.
 * These are a model of the selected native branches, not a game integration test.
 */
#include <stdio.h>
#include "../../src/native/physics/lap_guard_core.h"
typedef struct Fixture {
    LapGuardFlags flags;
    unsigned completed_laps;
    double best, last;
} Fixture;
static int same_flags(const LapGuardFlags *a, const LapGuardFlags *b) {
    return a->current == b->current && a->remembered == b->remembered &&
           a->last_computed == b->last_computed && a->next_timed == b->next_timed &&
           a->off_track == b->off_track;
}

/* 14130: CD6AFB..CD6B25. Rechecked 14200: CF107F..CF10AE
 * (EDI=1 on the selected off-track path). No native calls in this fixture. */
static void track_limits(LapGuardFlags *s) {
    int current = s->current;
    if (s->last_computed != current)
        s->remembered = current;
    if (current) {
        int value = s->off_track ? 1 : s->remembered;
        s->last_computed = value;
        s->current = value;
    }
}
/* Relevant branch gates: 0xCCD7F8 skips counting if zero; next-lap setup at
 * 0xCCE357..0xCCE373 calculates 1+(next_timed!=0), then next_timed=1.
 * Record updates here are fixture sentinels to detect an erroneously timed lap.
 */
static void cross_line(Fixture *f, double time) {
    if (f->flags.current)
        ++f->completed_laps;
    if (f->flags.current == 2) {
        f->last = time;
        if (time < f->best)
            f->best = time;
    }
    f->flags.current = 1 + (f->flags.next_timed != 0);
    f->flags.next_timed = 1;
}
#define REQUIRE(x)                                                                                 \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    unsigned tested = 0;
    for (int c = 0; c <= 2; c++)
        for (int r = 0; r <= 2; r++)
            for (int last = 0; last <= 2; last++)
                for (int next = 0; next <= 1; next++)
                    for (int off = 0; off <= 1; off++) {
                        Fixture f = {{c, r, last, (uint8_t)next, (uint8_t)off}, 8, 132.75, 136.5};
                        LapGuardFlags before = f.flags, after;
                        REQUIRE(lap_guard_prepare(&before, &after));
                        REQUIRE(same_flags(&before, &f.flags));
                        REQUIRE(after.next_timed == next && after.off_track == off);
                        f.flags = after;
                        for (int n = 0; n < 4; n++) {
                            track_limits(&f.flags);
                            REQUIRE(f.flags.current == 0);
                        }
                        cross_line(&f, 1.0); /* Physically interrupted attempt cannot count. */
                        REQUIRE(f.completed_laps == 8 && f.best == 132.75 && f.last == 136.5);
                        f.flags.off_track = 0;
                        track_limits(&f.flags);
                        REQUIRE(f.flags.current == (next ? 2 : 1));
                        if (!next) { /* Preserve an existing next-lap penalty for one lap. */
                            cross_line(&f, 1.0);
                            REQUIRE(f.best == 132.75);
                            track_limits(&f.flags);
                            REQUIRE(f.flags.current == 2);
                        }
                        cross_line(&f, 131.0);
                        REQUIRE(f.best == 131.0 && f.last == 131.0);
                        ++tested;
                    }
    /* Regression: touching only current causes the newly valid lap to be lost. */
    Fixture broken = {{2, 2, 2, 1, 0}, 8, 132.75, 136.5};
    broken.flags.current = 0;
    track_limits(&broken.flags);
    cross_line(&broken, 1.0);
    track_limits(&broken.flags);
    REQUIRE(broken.flags.current == 0);
    LapGuardFlags invalid = {3, 0, 0, 1, 0}, unchanged = {1, 1, 1, 1, 0}, out = unchanged;
    REQUIRE(!lap_guard_prepare(&invalid, &out));
    REQUIRE(same_flags(&out, &unchanged));
    REQUIRE(!lap_guard_prepare(NULL, &out));
    REQUIRE(!lap_guard_prepare(&unchanged, NULL));
    printf("PASS %u flag combinations; interrupted attempt excluded; record sentinels preserved; "
           "next valid lap recovers; prior penalty preserved; stale-cache regression detected; "
           "invalid input refused. MODEL ONLY, no live writes.\n",
           tested);
    return 0;
}
