/**
 * \file            nx_firmware_info.h
 * \brief           Nexus firmware information embedding
 * \author          Nexus Team
 *
 * This file provides firmware metadata embedding in a dedicated linker
 * section. Tools can extract version and build information from the
 * binary without executing the firmware.
 */

#ifndef NX_FIRMWARE_INFO_H
#define NX_FIRMWARE_INFO_H

#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Firmware information structure
 *
 * Contains firmware metadata that is placed in a dedicated linker section.
 * This allows external tools to extract version and build information
 * from the binary without executing the firmware.
 */
typedef struct nx_firmware_info_s {
    char product[32]; /**< Product name */
    char factory[16]; /**< Factory/vendor identifier */
    char date[12];    /**< Build date (__DATE__) */
    char time[12];    /**< Build time (__TIME__) */
    uint32_t version; /**< Version number (major.minor.patch.build) */
    uint32_t key;     /**< Firmware key/checksum value */
} nx_firmware_info_t;

/*---------------------------------------------------------------------------*/
/* Version Encoding Macros                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Encode version number as 32-bit value
 * \param[in]       major: Major version (0-255)
 * \param[in]       minor: Minor version (0-255)
 * \param[in]       patch: Patch version (0-255)
 * \param[in]       build: Build number (0-255)
 * \return          Encoded 32-bit version value
 *
 * Version format: [major:8][minor:8][patch:8][build:8]
 */
#define NX_VERSION_ENCODE(major, minor, patch, build)                          \
    (((uint32_t)(major) << 24) | ((uint32_t)(minor) << 16) |                   \
     ((uint32_t)(patch) << 8) | (uint32_t)(build))

/**
 * \brief           Extract major version from encoded value
 * \param[in]       ver: Encoded version value
 * \return          Major version (0-255)
 */
#define NX_VERSION_MAJOR(ver) (((ver) >> 24) & 0xFF)

/**
 * \brief           Extract minor version from encoded value
 * \param[in]       ver: Encoded version value
 * \return          Minor version (0-255)
 */
#define NX_VERSION_MINOR(ver) (((ver) >> 16) & 0xFF)

/**
 * \brief           Extract patch version from encoded value
 * \param[in]       ver: Encoded version value
 * \return          Patch version (0-255)
 */
#define NX_VERSION_PATCH(ver) (((ver) >> 8) & 0xFF)

/**
 * \brief           Extract build number from encoded value
 * \param[in]       ver: Encoded version value
 * \return          Build number (0-255)
 */
#define NX_VERSION_BUILD(ver) ((ver) & 0xFF)

/*---------------------------------------------------------------------------*/
/* Firmware Info Definition Macro                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Define firmware information in dedicated section
 * \param[in]       _product: Product name string (max 31 chars)
 * \param[in]       _factory: Factory/vendor identifier (max 15 chars)
 * \param[in]       _ver: Encoded version (use NX_VERSION_ENCODE)
 * \param[in]       _key: Firmware key/checksum value
 *
 * This macro defines a firmware_info structure and places it in the
 * .nx_fw_info linker section. The build date and time are automatically
 * captured using __DATE__ and __TIME__ macros.
 *
 * Example:
 * \code
 * NX_FIRMWARE_INFO_DEFINE(
 *     "Nexus Demo",
 *     "NEXUS",
 *     NX_VERSION_ENCODE(1, 0, 0, 0),
 *     0x12345678
 * );
 * \endcode
 */
#if defined(_MSC_VER)
/* MSVC: Define without section attribute for testing */
#define NX_FIRMWARE_INFO_DEFINE(_product, _factory, _ver, _key)                \
    const nx_firmware_info_t nx_firmware_info = {.product = _product,          \
                                                 .factory = _factory,          \
                                                 .date = __DATE__,             \
                                                 .time = __TIME__,             \
                                                 .version = _ver,              \
                                                 .key = _key}
#else
#define NX_FIRMWARE_INFO_DEFINE(_product, _factory, _ver, _key)                \
    NX_USED const nx_firmware_info_t nx_firmware_info NX_SECTION(              \
        ".nx_fw_info") = {.product = _product,                                 \
                          .factory = _factory,                                 \
                          .date = __DATE__,                                    \
                          .time = __TIME__,                                    \
                          .version = _ver,                                     \
                          .key = _key}
#endif

/*---------------------------------------------------------------------------*/
/* Public API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get firmware information pointer
 * \return          Pointer to firmware info, NULL if not defined
 *
 * Returns a pointer to the firmware information structure if it was
 * defined using NX_FIRMWARE_INFO_DEFINE, otherwise returns NULL.
 */
const nx_firmware_info_t* nx_get_firmware_info(void);

/**
 * \brief           Get firmware version as formatted string
 * \param[out]      buf: Output buffer for version string
 * \param[in]       size: Size of output buffer
 * \return          Number of characters written (excluding null terminator)
 *
 * Formats the firmware version as "major.minor.patch.build" string.
 * If firmware info is not defined, returns 0 and buffer is unchanged.
 *
 * Example output: "1.2.3.4"
 */
size_t nx_get_version_string(char* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* NX_FIRMWARE_INFO_H */
