/**
 * \file            w25q128jv.h
 * \brief           Reviewed serial NOR protocol without controller SDK types
 * \author          Nexus Team
 */
#ifndef NX_W25Q128JV_H
#define NX_W25Q128JV_H

#include "nexus/io/flash.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Winbond W25Q128JV 24-bit SPI commands and physical geometry. */
enum {
    NX_W25Q128JV_BYTES = 0x1000000U,
    NX_W25Q128JV_PAGE_BYTES = 256U,
    NX_W25Q128JV_ERASE_BYTES = 4096U,
    NX_W25Q128JV_ADDRESS_BITS = 24U,
    NX_W25Q128JV_QUAD_DUMMY_CYCLES = 8U,
    NX_W25Q128JV_READ_ID = 0x9fU,
    NX_W25Q128JV_READ = 0x03U,
    NX_W25Q128JV_QUAD_READ = 0x6bU,
    NX_W25Q128JV_STATUS1 = 0x05U,
    NX_W25Q128JV_STATUS2 = 0x35U,
    NX_W25Q128JV_STATUS3 = 0x15U,
    NX_W25Q128JV_WRITE_ENABLE = 0x06U,
    NX_W25Q128JV_WRITE_DISABLE = 0x04U,
    NX_W25Q128JV_PROGRAM = 0x02U,
    NX_W25Q128JV_ERASE = 0x20U
};

/** \brief One physical array; it does not authorize product reservations. */
extern const nx_flash_geometry_t nx_w25q128jv_geometry;

/** \brief Check the complete observed JEDEC identity without discovery.
 * \param[in] id: Three readable identity bytes from the selected transport.
 * \return True only for EF/40/18; no wiring or suffix qualification.
 * \note Pure bounded protocol check; does not retain the byte array.
 */
bool nx_w25q128jv_id_matches(const uint8_t id[3]);

/** \brief Adopt only an unsuspended device and any required existing QE bit.
 * \param[in] status2: Observed SR2 without status-register mutation.
 * \param[in] quad: Whether the selected read needs an already enabled QE bit.
 * \return SUCCESS or UNSUPPORTED; no resume, reset or protection changes.
 * \note Pure protocol check, independent of controller state and ownership.
 */
nx_result_t nx_w25q128jv_adopt(uint8_t status2, bool quad);

/** \brief Bound one physical pulse to a page or uniform erase unit.
 * \param[in] erase: True for an aligned erase, false for page programming.
 * \param[in] offset: Validated current byte offset in this physical array.
 * \param[in] remaining: Validated nonzero remaining request extent.
 * \return The next nonzero prefix, never crossing a program page.
 * \note Pure arithmetic; request borrowing belongs to the transport owner.
 */
size_t nx_w25q128jv_chunk(bool erase, uint32_t offset, size_t remaining);

/** \brief Classify one status observation before any new write pulse.
 * \param[in] command: STATUS1, STATUS2 or STATUS3 command identity.
 * \param[in] value: Observed byte from that exact register.
 * \return SUCCESS, BUSY, UNSUPPORTED protection/suspend or INVALID command.
 * \note Pure inline protocol decoding; no transport, retained state or policy.
 */
static inline nx_result_t nx_w25q128jv_write_status(uint8_t command,
                                                    uint8_t value) {
    if (command == NX_W25Q128JV_STATUS1) {
        if ((value & 1U) != 0U)
            return NX_ERROR_BUSY;
        return (value & 0x1cU) == 0U ? NX_SUCCESS : NX_ERROR_UNSUPPORTED;
    }
    if (command == NX_W25Q128JV_STATUS2)
        return (value & 0xc0U) == 0U ? NX_SUCCESS : NX_ERROR_UNSUPPORTED;
    if (command == NX_W25Q128JV_STATUS3)
        return (value & 4U) == 0U ? NX_SUCCESS : NX_ERROR_UNSUPPORTED;
    return NX_ERROR_INVALID;
}

#ifdef __cplusplus
}
#endif
#endif /* NX_W25Q128JV_H */
