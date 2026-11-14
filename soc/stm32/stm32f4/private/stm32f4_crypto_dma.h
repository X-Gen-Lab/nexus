/**
 * \file            stm32f4_crypto_dma.h
 * \brief           Private selected crypto DMA channels and memory admission
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_CRYPTO_DMA_H
#define NEXUS_STM32F4_CRYPTO_DMA_H
#include "nexus/io/dma.h"
#include "stm32f4xx_hal_dma.h"
#ifndef NEXUS_STM32_CRYPTO_DMA
#define NEXUS_STM32_CRYPTO_DMA 0U
#endif
#ifndef NEXUS_STM32_CIPHER_DMA
#define NEXUS_STM32_CIPHER_DMA 0U
#endif
#ifndef NEXUS_STM32_HASH_DMA
#define NEXUS_STM32_HASH_DMA 0U
#endif
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact reviewed DMA2 roles; no peripheral address is caller supplied.
 */
typedef enum {
    NX_STM32_CRYPTO_DMA_CIPHER_INPUT,
    NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT,
    NX_STM32_CRYPTO_DMA_HASH_INPUT
} nx_stm32_crypto_dma_role_t;
/** \brief Selected optional state; default IRQ owners never allocate it. */
typedef struct {
    DMA_HandleTypeDef handle;
    uint16_t words;
    uint16_t completed_words;
    bool captured;
    bool prepared;
} nx_stm32_crypto_dma_channel_t;
/** \brief Validate disjoint static metadata and exact F415/F417 DMA regions.
 * \param[in] owner: CPU owner with owner_bytes storage.
 * \param[in] owner_bytes: Complete mutable owner extent.
 * \param[in] config: Immutable outer configuration of config_bytes bytes.
 * \param[in] config_bytes: Complete outer configuration extent.
 * \param[in] connection: Immutable DMA connection of connection_bytes bytes.
 * \param[in] connection_bytes: Complete DMA connection extent.
 * \param[in] extension: Complete optional mutable extension.
 * \param[in] extension_bytes: Extension storage size.
 * \param[in] regions: Immutable selected memory facts.
 * \param[in] count: Number of regions, 1..8.
 * \return SUCCESS, INVALID or PERMISSION without effects or retention.
 */
nx_result_t nx_stm32_crypto_dma_metadata(
    const void* owner, size_t owner_bytes, const void* config,
    size_t config_bytes, const void* connection, size_t connection_bytes,
    const void* extension, size_t extension_bytes,
    const nx_dma_memory_region_t* regions, size_t count);
/** \brief Reject foreign live channel/vector state without register writes.
 * \param[in] channel: Fresh selected CPU-only channel storage.
 * \param[in] role: Exact reviewed DMA2 role.
 * \return SUCCESS, INVALID or BUSY; no channel is acquired.
 */
nx_result_t
nx_stm32_crypto_dma_admit(const nx_stm32_crypto_dma_channel_t* channel,
                          nx_stm32_crypto_dma_role_t role);
/** \brief Capture and initialize one disabled exact word-wide HAL channel.
 * \param[in,out] channel: Admitted selected storage, retained on failure.
 * \param[in] role: Same reviewed role throughout this lifetime.
 * \param[in] parent: Stable official transport Handle until complete stop.
 * \return SUCCESS or retained failure requiring stop; DMA2 clock is external.
 */
nx_result_t nx_stm32_crypto_dma_prepare(nx_stm32_crypto_dma_channel_t* channel,
                                        nx_stm32_crypto_dma_role_t role,
                                        void* parent);
/** \brief Verify stable descriptor facts while allowing real DMA progress.
 * \param[in] channel: Prepared channel after its official transport starts it.
 * \param[in] role: Original role.
 * \param[in] memory: Already admitted exact caller span.
 * \param[in] words: Number of complete words, 1..65535.
 * \return SUCCESS or IO; failed proof does not revoke the existing loan.
 */
nx_result_t nx_stm32_crypto_dma_verify(nx_stm32_crypto_dma_channel_t* channel,
                                       nx_stm32_crypto_dma_role_t role,
                                       const void* memory, size_t words);
/** \brief Dispatch latched DMA facts under the sole serialized task owner.
 * \param[in,out] channel: Prepared stable channel and official callbacks.
 * \param[in] role: Original role; NVIC remains disabled.
 * \return SUCCESS, STATE or IO; no memory/request completion is inferred.
 */
nx_result_t nx_stm32_crypto_dma_poll(nx_stm32_crypto_dma_channel_t* channel,
                                     nx_stm32_crypto_dma_role_t role);
/** \brief Join hardware before clearing addresses and callback references.
 * \param[in,out] channel: Original selected channel, retained on any failure.
 * \param[in] role: Original role.
 * \return SUCCESS or BUSY/IO/STATE; no shared DMA reset or gate mutation.
 */
nx_result_t nx_stm32_crypto_dma_withdraw(nx_stm32_crypto_dma_channel_t* channel,
                                         nx_stm32_crypto_dma_role_t role);
/** \brief Restore the admitted cold channel only after successful withdrawal.
 * \param[in,out] channel: Original selected storage, cleared only on success.
 * \param[in] role: Original role.
 * \return SUCCESS or retained failure; DMA2 clock must remain held.
 */
nx_result_t nx_stm32_crypto_dma_stop(nx_stm32_crypto_dma_channel_t* channel,
                                     nx_stm32_crypto_dma_role_t role);
#ifdef __cplusplus
}
#endif
#endif
