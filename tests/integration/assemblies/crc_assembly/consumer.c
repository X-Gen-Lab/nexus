/**
 * \file            consumer.c
 * \brief           Real static CRC factory and physical chip information links
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/chip.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Linkage fixture; neither execution nor silicon is qualified here. */
int main(void) {
    nx_chip_info_t info;
    uint8_t uid[16];
    size_t uid_bytes = 0U;
    if (nx_chip_info_read(&info) != NX_SUCCESS ||
        nx_chip_uid_read(uid, sizeof(uid), &uid_bytes) != NX_SUCCESS ||
        uid_bytes == 0U) {
        return 1;
    }
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS || start.cleanup != NX_SUCCESS) {
        return 2;
    }
    const nx_crc_port_t* crc = nx_factory_crc_ref(NX_CRC_REF_SIGNATURE);
    const uint32_t words[] = {0x31323334U, 0x35363738U, 0x39000000U};
    uint32_t signature = 0U;
    nx_time_us_t deadline;
    nx_result_t result =
        nx_deadline_finite_after(nx_time_now_us(), 1000U, &deadline);
    if (result == NX_SUCCESS) {
        result = nx_crc_port_compute(crc, words, 3U, deadline, &signature);
    }
    nx_result_t stopped = nx_platform_stop();
    return result == NX_SUCCESS && signature == 0xae24e09dU &&
                   stopped == NX_SUCCESS
               ? 0
               : 3;
}
