/**
 * \file            mcuboot_boot_call.h
 * \brief           C11 boundary for the official normal boot entry.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_MCUBOOT_BOOT_CALL_H
#define NEXUS_TEST_MCUBOOT_BOOT_CALL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t offset;
    uint16_t header_size;
    uint8_t major;
    uint8_t minor;
} nx_test_mcuboot_selected_t;
int nx_test_mcuboot_boot(nx_test_mcuboot_selected_t* selected);
#ifdef __cplusplus
}
#endif
#endif
