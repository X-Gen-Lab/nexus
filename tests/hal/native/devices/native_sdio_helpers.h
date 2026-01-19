/**
 * \file            native_sdio_helpers.h
 * \brief           Native SDIO test helpers
 * \author          Nexus Team
 */

#ifndef NATIVE_SDIO_HELPERS_H
#define NATIVE_SDIO_HELPERS_H

#include "hal/interface/nx_sdio.h"
#include "hal/nx_factory.h"
#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

nx_status_t native_sdio_get_state(uint8_t index, bool* initialized,
                                  bool* suspended);
nx_status_t native_sdio_set_card_present(uint8_t index, bool present);
bool native_sdio_is_card_present(uint8_t index);
nx_status_t native_sdio_get_block_data(uint8_t index, uint32_t block,
                                       uint8_t* data);
void native_sdio_reset_all(void);
void native_sdio_reset(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_SDIO_HELPERS_H */
