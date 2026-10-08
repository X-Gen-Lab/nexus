#include "nx_init.h"
#include <cassert>

static int order[3];
static unsigned calls;
static int app() { order[calls++] = 6; return 0; }
static int driver() { order[calls++] = 4; return -37; }
static int board() { order[calls++] = 1; return 0; }

// Deliberately declare out of order: the linker must order by init level.
NX_INIT_APP_EXPORT(app);
NX_INIT_DRIVER_EXPORT(driver);
NX_INIT_BOARD_EXPORT(board);

int main() {
    assert(nx_init_run() == NX_ERR_GENERIC);
    assert(calls == 3);
    assert(order[0] == 1 && order[1] == 4 && order[2] == 6);
    nx_init_stats_t stats{};
    assert(nx_init_get_stats(&stats) == NX_OK);
    assert(stats.total_count == 3 && stats.success_count == 2);
    assert(stats.fail_count == 1 && stats.last_error == -37);
    assert(!nx_init_is_complete());
    assert(nx_init_run() == NX_ERR_GENERIC);
    assert(calls == 3); // No callback is executed twice.
    assert(nx_init_get_stats(&stats) == NX_OK && stats.fail_count == 1);
    return 0;
}
