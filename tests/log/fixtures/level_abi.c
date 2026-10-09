#include "log/log.h"

/* C accepts integer-to-enum conversion. Keeping it on this side of the ABI
 * exercises production range validation without a C++ enum load whose value
 * is outside that enum's representable range. Sanitizers remain enabled. */
log_status_t log_test_set_level_integer(int value) {
    return log_set_level((log_level_t)value);
}
