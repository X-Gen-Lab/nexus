/** STM32F4 DMA termination and owned-handle callback routing. */
#include "stm32_spi.h"

static stm32_spi_impl_t* registry[STM32_SPI_MAX_BUSES];
void spi_registry_add(stm32_spi_impl_t* b) {
    uint32_t saved = spi_critical_enter();
    for (unsigned i = 0; i < STM32_SPI_MAX_BUSES; ++i) {
        if (registry[i] == b) { spi_critical_leave(saved); return; }
        if (!registry[i]) { registry[i] = b; spi_critical_leave(saved); return; }
    }
    /* Only SPI1..SPI6 exist in the registered supported range. */
    b->state->fault = true;
    spi_critical_leave(saved);
}
void spi_registry_remove(stm32_spi_impl_t* b) {
    uint32_t saved = spi_critical_enter();
    for (unsigned i = 0; i < STM32_SPI_MAX_BUSES; ++i)
        if (registry[i] == b) registry[i] = NULL;
    spi_critical_leave(saved);
}
static stm32_spi_impl_t* find_bus(SPI_HandleTypeDef* hspi) {
    for (unsigned i = 0; i < STM32_SPI_MAX_BUSES; ++i)
        if (registry[i] && &registry[i]->hspi == hspi) return registry[i];
    return NULL;
}
bool spi_publish(stm32_spi_impl_t* b, nx_status_t result) {
    uint32_t saved = spi_critical_enter();
    bool won = b->phase == STM32_SPI_WAITING;
    if (won) {
        b->result = result;
        __DMB();
        b->phase = STM32_SPI_TERMINAL;
    }
    spi_critical_leave(saved);
    return won;
}
static void completed(SPI_HandleTypeDef* hspi, nx_status_t result) {
    /* Never container_of() an arbitrary vendor handle owned by another driver. */
    stm32_spi_impl_t* b = find_bus(hspi);
    if (!b || !spi_publish(b, result)) return;
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    if (b->dma_sem) {
        /* HAL invokes these from DMA/SPI IRQ handlers. */
        if (__get_IPSR()) (void)osal_sem_give_from_isr(b->dma_sem);
        else (void)osal_sem_give(b->dma_sem);
    }
#endif
}
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) { completed(hspi, NX_OK); }
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi) { completed(hspi, NX_OK); }
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi) { completed(hspi, NX_OK); }
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* hspi) { completed(hspi, NX_ERR_DMA_TRANSFER); }

nx_status_t spi_dma_init(stm32_spi_impl_t* b) {
    if (!b) return NX_ERR_INVALID_PARAM;
#if defined(STM32F4xx) || defined(STM32F407xx)
    /* A genuine board prepare hook initializes and links DMA streams and IRQs.
     * No successful placeholder initialization is allowed here. */
    if ((b->dma_tx_enabled && (!b->hspi.hdmatx || !b->hspi.hdmatx->Instance ||
                               b->hspi.hdmatx->Parent != &b->hspi)) ||
        (b->dma_rx_enabled && (!b->hspi.hdmarx || !b->hspi.hdmarx->Instance ||
                               b->hspi.hdmarx->Parent != &b->hspi)))
        return NX_ERR_DMA_CONFIG;
    return NX_OK;
#else
    return NX_ERR_NOT_SUPPORTED;
#endif
}

NX_WEAK NX_NORETURN void stm32_spi_dma_failstop(stm32_spi_impl_t* b) {
    (void)b;
    /* A device that cannot stop a DMA master must reset/halt, never release the
     * caller's buffer. Products may override this with a recorded safe reset. */
    __disable_irq();
    for (;;) { __NOP(); }
}
#if defined(STM32F4xx) || defined(STM32F407xx)
static nx_status_t drain_stream(stm32_spi_impl_t* b, DMA_HandleTypeDef* dma) {
    if (!dma || !dma->Instance) return NX_OK;
    nx_status_t result = NX_OK;
    if (dma->State == HAL_DMA_STATE_BUSY && HAL_DMA_Abort(dma) != HAL_OK)
        result = NX_ERR_HARDWARE;
    uint32_t saved = spi_critical_enter();
    /* Abort can fail or report NO_XFER. Independently settle hardware ownership. */
    __HAL_DMA_DISABLE_IT(dma, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE | DMA_IT_DME | DMA_IT_FE);
    __HAL_DMA_DISABLE(dma);
    __DSB();
    if (dma->Instance->CR & DMA_SxCR_EN) {
        spi_critical_leave(saved);
        stm32_spi_dma_failstop(b);
    }
    __HAL_DMA_CLEAR_FLAG(dma, __HAL_DMA_GET_TC_FLAG_INDEX(dma) |
                         __HAL_DMA_GET_HT_FLAG_INDEX(dma) |
                         __HAL_DMA_GET_TE_FLAG_INDEX(dma) |
                         __HAL_DMA_GET_DME_FLAG_INDEX(dma) |
                         __HAL_DMA_GET_FE_FLAG_INDEX(dma));
    dma->XferCpltCallback = NULL;
    dma->XferHalfCpltCallback = NULL;
    dma->XferErrorCallback = NULL;
    dma->XferAbortCallback = NULL;
    dma->State = HAL_DMA_STATE_READY;
    dma->Lock = HAL_UNLOCKED;
    spi_critical_leave(saved);
    return result;
}
#endif
nx_status_t spi_dma_drain(stm32_spi_impl_t* b) {
    if (!b) return NX_ERR_INVALID_PARAM;
#if defined(STM32F4xx) || defined(STM32F407xx)
    /* Cut DMA requests before abort, so abort cannot spend a vendor timeout
     * waiting for an external slave/serial word. Streams are settled below. */
    uint32_t saved = spi_critical_enter();
    CLEAR_BIT(b->hspi.Instance->CR2, SPI_CR2_TXDMAEN | SPI_CR2_RXDMAEN |
               SPI_CR2_TXEIE | SPI_CR2_RXNEIE | SPI_CR2_ERRIE);
    __HAL_SPI_DISABLE(&b->hspi);
    spi_critical_leave(saved);
    nx_status_t result = HAL_SPI_Abort(&b->hspi) == HAL_OK ? NX_OK : NX_ERR_HARDWARE;
    nx_status_t tx = drain_stream(b, b->hspi.hdmatx);
    nx_status_t rx = drain_stream(b, b->hspi.hdmarx);
    b->hspi.pTxBuffPtr = NULL;
    b->hspi.pRxBuffPtr = NULL;
    b->hspi.TxXferCount = b->hspi.RxXferCount = 0;
    __DSB();
    return result != NX_OK ? result : tx != NX_OK ? tx : rx;
#else
    return NX_ERR_NOT_SUPPORTED;
#endif
}
nx_status_t spi_dma_deinit(stm32_spi_impl_t* b) {
    if (!b) return NX_ERR_INVALID_PARAM;
    return spi_dma_drain(b);
}
