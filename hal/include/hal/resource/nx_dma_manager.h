/**
 * \file            nx_dma_manager.h
 * \brief           DMA resource manager interface
 * \author          Nexus Team
 */

#ifndef NX_DMA_MANAGER_H
#define NX_DMA_MANAGER_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           DMA channel handle (opaque)
 */
typedef struct nx_dma_channel_s nx_dma_channel_t;

/**
 * \brief           DMA transfer direction enumeration
 */
typedef enum nx_dma_direction_e {
    NX_DMA_DIR_PERIPH_TO_MEM = 0, /**< Peripheral to memory */
    NX_DMA_DIR_MEM_TO_PERIPH,     /**< Memory to peripheral */
    NX_DMA_DIR_MEM_TO_MEM,        /**< Memory to memory */
} nx_dma_direction_t;

/**
 * \brief           DMA transfer complete callback type
 * \param[in]       user_data: User data pointer
 * \param[in]       result: Transfer result status
 */
typedef void (*nx_dma_callback_t)(void* user_data, nx_status_t result);

/**
 * \brief           DMA request configuration structure
 */
typedef struct nx_dma_request_s {
    uint32_t periph_addr;       /**< Peripheral address */
    uint32_t memory_addr;       /**< Memory address */
    uint32_t transfer_count;    /**< Transfer count */
    uint8_t periph_width;       /**< Peripheral data width: 8/16/32 */
    uint8_t memory_width;       /**< Memory data width: 8/16/32 */
    bool periph_inc;            /**< Peripheral address increment */
    bool memory_inc;            /**< Memory address increment */
    bool circular;              /**< Circular mode flag */
    uint8_t priority;           /**< Priority: 0-3 */
    nx_dma_callback_t callback; /**< Completion callback */
    void* user_data;            /**< User data for callback */
} nx_dma_request_t;

/**
 * \brief           DMA manager interface
 */
typedef struct nx_dma_manager_s nx_dma_manager_t;
struct nx_dma_manager_s {
    /**
     * \brief           Allocate DMA channel for peripheral
     * \param[in]       self: DMA manager instance
     * \param[in]       periph: Peripheral identifier
     * \return          DMA channel handle, NULL on failure
     */
    nx_dma_channel_t* (*alloc)(nx_dma_manager_t* self, uint32_t periph);

    /**
     * \brief           Free DMA channel
     * \param[in]       self: DMA manager instance
     * \param[in]       ch: DMA channel handle
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*free)(nx_dma_manager_t* self, nx_dma_channel_t* ch);

    /**
     * \brief           Start DMA transfer
     * \param[in]       ch: DMA channel handle
     * \param[in]       req: DMA request configuration
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*start)(nx_dma_channel_t* ch, const nx_dma_request_t* req);

    /**
     * \brief           Stop DMA transfer
     * \param[in]       ch: DMA channel handle
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*stop)(nx_dma_channel_t* ch);

    /**
     * \brief           Get remaining transfer count
     * \param[in]       ch: DMA channel handle
     * \return          Remaining transfer count
     */
    uint32_t (*get_remaining)(nx_dma_channel_t* ch);
};

/**
 * \brief           Get DMA manager singleton instance
 * \return          DMA manager pointer
 */
nx_dma_manager_t* nx_dma_manager_get(void);

#ifdef __cplusplus
}
#endif

#endif /* NX_DMA_MANAGER_H */
