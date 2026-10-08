/** STM32F4DISCOVERY (MB997) SPI1 fixture wiring, pending electrical HIL.
 * PA5/PA6/PA7 = SCK/MISO/MOSI, PB0/PB1 = logical slave CS 0/1.
 * Board revision and external slave wiring must match the evidence record. */
#include "stm32_spi_resource.h"
#include "hal/resource/nx_isr_manager.h"

static DMA_HandleTypeDef dma_tx, dma_rx;
static nx_isr_manager_t* manager;
static bool tx_connected, rx_connected, spi_connected;
static void tx_irq(void* context) { HAL_DMA_IRQHandler(context); }
static void rx_irq(void* context) { HAL_DMA_IRQHandler(context); }
static void spi_irq(void* context) { HAL_SPI_IRQHandler(context); }

nx_status_t stm32_spi_board_prepare(const stm32_spi_board_port_t* port) {
    if (!port || !port->handle || port->handle->Instance != SPI1 || port->instance != 1) return NX_ERR_NOT_SUPPORTED;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    /* Set output latch inactive before enabling GPIO output. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_SET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio);
    if (!port->dma_tx_enabled && !port->dma_rx_enabled) return NX_OK;
    __HAL_RCC_DMA2_CLK_ENABLE();
    dma_tx.Instance = DMA2_Stream3;
    dma_tx.Init.Channel = DMA_CHANNEL_3;
    dma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    dma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    dma_tx.Init.MemInc = DMA_MINC_ENABLE;
    dma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    dma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    dma_tx.Init.Mode = DMA_NORMAL;
    dma_tx.Init.Priority = DMA_PRIORITY_HIGH;
    dma_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    dma_rx.Init = dma_tx.Init;
    dma_rx.Instance = DMA2_Stream0;
    dma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    if (HAL_DMA_Init(&dma_tx) != HAL_OK || HAL_DMA_Init(&dma_rx) != HAL_OK) {
        stm32_spi_board_release(port);
        return NX_ERR_DMA_CONFIG;
    }
    __HAL_LINKDMA(port->handle, hdmatx, dma_tx);
    __HAL_LINKDMA(port->handle, hdmarx, dma_rx);
    manager = nx_isr_manager_get();
    if (!manager) { stm32_spi_board_release(port); return NX_ERR_NO_RESOURCE; }
    /* Priority 5 is inside the default FreeRTOS syscall-safe range 5..15.
     * A product changing configMAX_SYSCALL_INTERRUPT_PRIORITY must review this. */
    nx_status_t r = manager->connect(manager, DMA2_Stream3_IRQn, tx_irq, &dma_tx, 5);
    if (r == NX_OK) tx_connected = true;
    if (r == NX_OK) r = manager->connect(manager, DMA2_Stream0_IRQn, rx_irq, &dma_rx, 5);
    if (r == NX_OK) rx_connected = true;
    if (r == NX_OK) r = manager->connect(manager, SPI1_IRQn, spi_irq, port->handle, 5);
    if (r == NX_OK) spi_connected = true;
    if (r != NX_OK) stm32_spi_board_release(port);
    return r;
}
nx_status_t stm32_spi_board_select(uint8_t instance, uint8_t cs, bool active) {
    if (instance != 1 || cs > 1) return NX_ERR_INVALID_PARAM;
    HAL_GPIO_WritePin(GPIOB, cs == 0 ? GPIO_PIN_0 : GPIO_PIN_1,
                       active ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return NX_OK;
}
uint32_t stm32_spi_board_clock_hz(uint8_t instance) {
    return instance == 1 ? HAL_RCC_GetPCLK2Freq() : 0;
}
void stm32_spi_board_release(const stm32_spi_board_port_t* port) {
    if (!port || !port->handle || port->handle->Instance != SPI1 || port->instance != 1) return;
    if (manager) {
        if (tx_connected) (void)manager->disconnect(manager, DMA2_Stream3_IRQn);
        if (rx_connected) (void)manager->disconnect(manager, DMA2_Stream0_IRQn);
        if (spi_connected) (void)manager->disconnect(manager, SPI1_IRQn);
    }
    tx_connected = rx_connected = spi_connected = false;
    HAL_NVIC_ClearPendingIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_ClearPendingIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_ClearPendingIRQ(SPI1_IRQn);
    if (dma_tx.Instance) (void)HAL_DMA_DeInit(&dma_tx);
    if (dma_rx.Instance) (void)HAL_DMA_DeInit(&dma_rx);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_SET);
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0 | GPIO_PIN_1);
    __HAL_RCC_SPI1_CLK_DISABLE();
    port->handle->hdmatx = port->handle->hdmarx = NULL;
}
