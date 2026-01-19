/**
 * \file            nx_dma_native_test.h
 * \brief           Native platform DMA manager test interfaces
 * \author          Nexus Team
 */

#ifndef NX_DMA_NATIVE_TEST_H
#define NX_DMA_NATIVE_TEST_H

#include "hal/nx_types.h"
#include "hal/resource/nx_dma_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Test Interface Functions                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get number of allocated DMA channels
 * \param[in]       dma_index: DMA controller index
 * \return          Number of allocated channels
 */
uint8_t nx_dma_test_get_allocated_count(uint8_t dma_index);

/**
 * \brief           Check if a specific channel is allocated
 * \param[in]       dma_index: DMA controller index
 * \param[in]       channel: Channel number
 * \return          true if allocated, false otherwise
 */
bool nx_dma_test_is_channel_allocated(uint8_t dma_index, uint8_t channel);

/**
 * \brief           Reset all DMA channels
 * \details         Releases all allocated channels and resets state
 */
void nx_dma_test_reset_all(void);

/**
 * \brief           Simulate DMA transfer completion
 * \param[in]       channel: DMA channel pointer
 * \details         Triggers the completion callback for testing
 */
void nx_dma_test_simulate_complete(nx_dma_channel_t* channel);

#ifdef __cplusplus
}
#endif

#endif /* NX_DMA_NATIVE_TEST_H */
