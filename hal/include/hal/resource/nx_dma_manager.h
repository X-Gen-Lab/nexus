/**
 * \file            nx_dma_manager.h
 * \brief           DMA management interface
 * \author          Nexus Team
 */

#ifndef NX_DMA_MANAGER_H
#define NX_DMA_MANAGER_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Type Definitions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           DMA transfer complete callback function type
 */
typedef void (*nx_dma_callback_t)(void* user_data);

/**
 * \brief           DMA transfer configuration
 */
typedef struct nx_dma_config_s {
    void* src_addr;     /**< Source address */
    void* dst_addr;     /**< Destination address */
    size_t size;        /**< Transfer size */
    uint8_t src_inc;    /**< Source address increment: 0=no, 1=yes */
    uint8_t dst_inc;    /**< Destination address increment: 0=no, 1=yes */
    uint8_t data_width; /**< Data width: 1=byte, 2=half-word, 4=word */
    bool circular;      /**< Circular mode */
} nx_dma_config_t;

/**
 * \brief           DMA channel interface
 */
typedef struct nx_dma_channel_s nx_dma_channel_t;
struct nx_dma_channel_s {
    /**
     * \brief           Configure DMA transfer parameters
     * \param[in]       self: Channel interface pointer
     * \param[in]       cfg: DMA configuration structure
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*configure)(nx_dma_channel_t* self,
                             const nx_dma_config_t* cfg);

    /**
     * \brief           Start DMA transfer
     * \param[in]       self: Channel interface pointer
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*start)(nx_dma_channel_t* self);

    /**
     * \brief           Stop DMA transfer
     * \param[in]       self: Channel interface pointer
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*stop)(nx_dma_channel_t* self);

    /**
     * \brief           Get remaining transfer count
     * \param[in]       self: Channel interface pointer
     * \return          Remaining transfer count
     */
    size_t (*get_remaining)(nx_dma_channel_t* self);

    /**
     * \brief           Set transfer complete callback
     * \param[in]       self: Channel interface pointer
     * \param[in]       callback: Callback function
     * \param[in]       user_data: User data passed to callback
     * \return          NX_OK on success, error code otherwise
     */
    nx_status_t (*set_callback)(nx_dma_channel_t* self,
                                nx_dma_callback_t callback, void* user_data);
};

/*---------------------------------------------------------------------------*/
/* Function Declarations                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Allocate a DMA channel
 * \param[in]       dma_index: DMA controller index
 * \param[in]       channel: Channel number
 * \return          DMA channel interface pointer, NULL if allocation fails
 */
nx_dma_channel_t* nx_dma_allocate_channel(uint8_t dma_index, uint8_t channel);

/**
 * \brief           Release a DMA channel
 * \param[in]       channel: DMA channel interface pointer
 * \return          NX_OK on success, error code otherwise
 */
nx_status_t nx_dma_release_channel(nx_dma_channel_t* channel);

#ifdef __cplusplus
}
#endif

#endif /* NX_DMA_MANAGER_H */
