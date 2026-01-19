/**
 * \file            native_option_bytes_helpers.h
 * \brief           Native Option Bytes test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_OPTION_BYTES_HELPERS_H
#define NATIVE_OPTION_BYTES_HELPERS_H

#include "hal/interface/nx_option_bytes.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

nx_status_t native_option_bytes_get_state(uint8_t index, bool* initialized,
                                          bool* suspended);
nx_status_t native_option_bytes_set_write_protection(uint8_t index,
                                                     bool enabled);
nx_status_t native_option_bytes_has_pending_changes(uint8_t index,
                                                    bool* has_pending);
void native_option_bytes_reset_all(void);
void native_option_bytes_reset(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_OPTION_BYTES_HELPERS_H */
