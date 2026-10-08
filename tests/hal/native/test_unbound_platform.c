/* Linking HAL alone must not claim that an absent platform started. */
#include "hal/nx_hal.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(!nx_hal_is_initialized());
    assert(nx_hal_init() == NX_ERR_NOT_SUPPORTED);
    assert(!nx_hal_is_initialized());
    assert(nx_hal_init() == NX_ERR_NOT_SUPPORTED);
    assert(!nx_hal_is_initialized());
    puts("Unbound platform startup rejected");
    return 0;
}
