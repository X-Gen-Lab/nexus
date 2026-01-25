/**
 * \file            nx_flash.h
 * \brief           Internal Flash interface definition
 * \author          Nexus Team
 */

#ifndef NX_FLASH_H
#define NX_FLASH_H

#include "hal/interface/nx_lifecycle.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Internal Flash Interface                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Internal Flash interface
 *
 * Provides access to internal flash memory for data storage.
 * Supports read, write, erase operations with alignment handling,
 * write protection (lock/unlock), and lifecycle management.
 */
typedef struct nx_internal_flash_s nx_internal_flash_t;
struct nx_internal_flash_s {
    /**
     * \brief           Read data from flash
     * \param[in]       self: Flash interface pointer
     * \param[in]       addr: Flash address to read from
     * \param[out]      data: Buffer to store read data
     * \param[in]       len: Number of bytes to read
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*read)(nx_internal_flash_t* self, uint32_t addr, uint8_t* data,
                        size_t len);

    /**
     * \brief           Write data to flash with alignment handling
     * \param[in]       self: Flash interface pointer
     * \param[in]       addr: Flash address to write to
     * \param[in]       data: Data buffer to write
     * \param[in]       len: Number of bytes to write
     * \return          NX_OK on success, error code otherwise
     * \note            Flash must be unlocked and erased before writing
     */
    nx_status_t (*write)(nx_internal_flash_t* self, uint32_t addr,
                         const uint8_t* data, size_t len);

    /**
     * \brief           Erase flash pages
     * \param[in]       self: Flash interface pointer
     * \param[in]       addr: Start address of pages to erase
     * \param[in]       size: Size in bytes to erase (rounded up to page
     *                  boundary)
     * \return          NX_OK on success, error code otherwise
     * \note            Flash must be unlocked before erasing
     */
    nx_status_t (*erase)(nx_internal_flash_t* self, uint32_t addr, size_t size);

    /**
     * \brief           Get flash page size
     * \param[in]       self: Flash interface pointer
     * \return          Page size in bytes
     */
    size_t (*get_page_size)(nx_internal_flash_t* self);

    /**
     * \brief           Get minimum write unit size
     * \param[in]       self: Flash interface pointer
     * \return          Minimum write unit in bytes (e.g., 1, 2, 4, 8)
     */
    size_t (*get_write_unit)(nx_internal_flash_t* self);

    /**
     * \brief           Lock flash for write protection
     * \param[in]       self: Flash interface pointer
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*lock)(nx_internal_flash_t* self);

    /**
     * \brief           Unlock flash for write/erase operations
     * \param[in]       self: Flash interface pointer
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*unlock)(nx_internal_flash_t* self);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: Flash interface pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_internal_flash_t* self);
};

/*---------------------------------------------------------------------------*/
/* Flash Initialization Macro                                                */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize internal flash interface
 * \param[in]       p: Pointer to nx_internal_flash_t structure
 * \param[in]       _read: Read function pointer
 * \param[in]       _write: Write function pointer
 * \param[in]       _erase: Erase function pointer
 * \param[in]       _get_page_size: Get page size function pointer
 * \param[in]       _get_write_unit: Get write unit function pointer
 * \param[in]       _lock: Lock function pointer
 * \param[in]       _unlock: Unlock function pointer
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 */
#define NX_INIT_INTERNAL_FLASH(p, _read, _write, _erase, _get_page_size,       \
                               _get_write_unit, _lock, _unlock,                \
                               _get_lifecycle)                                 \
    do {                                                                       \
        (p)->read = (_read);                                                   \
        (p)->write = (_write);                                                 \
        (p)->erase = (_erase);                                                 \
        (p)->get_page_size = (_get_page_size);                                 \
        (p)->get_write_unit = (_get_write_unit);                               \
        (p)->lock = (_lock);                                                   \
        (p)->unlock = (_unlock);                                               \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        NX_ASSERT((p)->read != NULL);                                          \
        NX_ASSERT((p)->write != NULL);                                         \
        NX_ASSERT((p)->erase != NULL);                                         \
        NX_ASSERT((p)->get_page_size != NULL);                                 \
        NX_ASSERT((p)->get_write_unit != NULL);                                \
        NX_ASSERT((p)->lock != NULL);                                          \
        NX_ASSERT((p)->unlock != NULL);                                        \
        NX_ASSERT((p)->get_lifecycle != NULL);                                 \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NX_FLASH_H */
