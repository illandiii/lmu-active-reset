#include "../../src/native/runtime/reset_host_policy.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main() {
    // No four/five-minute session cap, nor an implicit checkpoint time-to-live.
    for (uint64_t now :
         {uint64_t{0}, uint64_t{300001}, uint64_t{86400000}, uint64_t{30} * 86400000}) {
        assert(reset_host_controller_alive(now, now));
        if (now >= 2000)
            assert(reset_host_controller_alive(now, now - 2000));
        if (now > 2000)
            assert(!reset_host_controller_alive(now, now - 2001));
        assert(!reset_host_controller_alive(now, now + 1));
    }
    puts("PASS indefinite host lifetime; controller-loss watchdog still enforced.");
}
