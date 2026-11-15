/**
 * \file            crc8.c
 * \brief           Dallas/Maxim wire-order CRC8 without retained state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/onewire.h"
uint8_t nx_onewire_crc8(const uint8_t* bytes, size_t length) {
    if (bytes == NULL || length > UINTPTR_MAX - (uintptr_t)bytes)
        return 0U;
    uint8_t crc = 0U;
    for (size_t i = 0U; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0U; bit < 8U; ++bit)
            crc = (uint8_t)((crc >> 1U) ^ ((crc & 1U) ? 0x8cU : 0U));
    }
    return crc;
}
