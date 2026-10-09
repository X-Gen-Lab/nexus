/** STM32F4DISCOVERY (MB997) SPI1 fixture wiring, pending electrical HIL.
 * PA5/PA6/PA7 = SCK/MISO/MOSI, PB0/PB1 = logical slave CS 0/1.
 * Board revision and external slave wiring must match the evidence record. */
#include "stm32_spi_resource.h"
#include "hal/resource/nx_isr_manager.h"

static DMA_HandleTypeDef dma_tx, dma_rx;
static nx_isr_manager_t* manager;
static SPI_HandleTypeDef* bound_handle;
static bool tx_connected, rx_connected, spi_connected;
static bool tx_dma_owned, rx_dma_owned, pins_owned, clock_owned;
static void tx_irq(void* context) { HAL_DMA_IRQHandler(context); }
static void rx_irq(void* context) { HAL_DMA_IRQHandler(context); }
static void spi_irq(void* context) { HAL_SPI_IRQHandler(context); }

nx_status_t stm32_spi_board_prepare(const stm32_spi_board_port_t* port) {
    if (!port || !port->handle || port->handle->Instance != SPI1 || port->instance != 1) return NX_ERR_NOT_SUPPORTED;
    /* A partial attempt is still owned: only release may settle it. Do not
     * overwrite the static handles or an unrelated SoC DMA binding. */
    if (bound_handle) return NX_ERR_BUSY;
    if (port->handle->hdmatx || port->handle->hdmarx) return NX_ERR_RESOURCE_BUSY;
    nx_isr_manager_t* selected_manager = NULL;
    if (port->dma_tx_enabled || port->dma_rx_enabled) {
        selected_manager = nx_isr_manager_get();
        if (!selected_manager) return NX_ERR_NO_RESOURCE;
        if (!selected_manager->connect || !selected_manager->disconnect)
            return NX_ERR_NOT_SUPPORTED;
    }
    bound_handle = port->handle;
    manager = selected_manager;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    clock_owned = true;
    pins_owned = true;
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
    /* HAL_DMA_Init can fail after programming the stream. Record each attempt
     * before entering the vendor function, not only its successful result. */
    tx_dma_owned = true;
    __HAL_LINKDMA(port->handle, hdmatx, dma_tx);
    if (HAL_DMA_Init(&dma_tx) != HAL_OK) return NX_ERR_DMA_CONFIG;
    rx_dma_owned = true;
    __HAL_LINKDMA(port->handle, hdmarx, dma_rx);
    if (HAL_DMA_Init(&dma_rx) != HAL_OK) return NX_ERR_DMA_CONFIG;
    /* Priority 5 is inside the default FreeRTOS syscall-safe range 5..15.
     * A product changing configMAX_SYSCALL_INTERRUPT_PRIORITY must review this. */
    nx_status_t r = manager->connect(manager, DMA2_Stream3_IRQn, tx_irq, &dma_tx, 5);
    if (r == NX_OK) tx_connected = true;
    if (r == NX_OK) r = manager->connect(manager, DMA2_Stream0_IRQn, rx_irq, &dma_rx, 5);
    if (r == NX_OK) rx_connected = true;
    if (r == NX_OK) r = manager->connect(manager, SPI1_IRQn, spi_irq, port->handle, 5);
    if (r == NX_OK) spi_connected = true;
    /* The controller owns the cleanup transaction. Returning the original
     * error leaves every attempted resource available for a truthful retry. */
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
static nx_status_t disconnect_owned(uint32_t irq, bool* connected) {
    if (!*connected) return NX_OK;
    if (!manager || !manager->disconnect) return NX_ERR_INVALID_STATE;
    nx_status_t status = manager->disconnect(manager, irq);
    if (status == NX_OK) {
        *connected = false;
        HAL_NVIC_ClearPendingIRQ((IRQn_Type)irq);
    }
    return status;
}
static nx_status_t release_dma(DMA_HandleTypeDef* dma, bool* owned,
                                DMA_HandleTypeDef** binding) {
    if (!*owned) return NX_OK;
    HAL_StatusTypeDef status = HAL_DMA_DeInit(dma);
    if (status != HAL_OK) return status == HAL_BUSY ? NX_ERR_BUSY :
        status == HAL_TIMEOUT ? NX_ERR_TIMEOUT : NX_ERR_IO;
    *owned = false;
    if (*binding == dma) *binding = NULL;
    dma->Instance = NULL;
    dma->Parent = NULL;
    return NX_OK;
}
nx_status_t stm32_spi_board_release(const stm32_spi_board_port_t* port) {
    if (!port || !port->handle || port->handle->Instance != SPI1 || port->instance != 1)
        return NX_ERR_NOT_SUPPORTED;
    if (!bound_handle) return NX_OK;
    if (port->handle != bound_handle) return NX_ERR_RESOURCE_BUSY;
    nx_status_t result = disconnect_owned(DMA2_Stream3_IRQn, &tx_connected);
    nx_status_t status = disconnect_owned(DMA2_Stream0_IRQn, &rx_connected);
    if (result == NX_OK) result = status;
    status = disconnect_owned(SPI1_IRQn, &spi_connected);
    if (result == NX_OK) result = status;
    /* Any connected callback can still borrow the SPI/DMA context. Successful
     * independent disconnects stay released; do not destroy a retained one. */
    if (result != NX_OK) return result;
    result = release_dma(&dma_tx, &tx_dma_owned, &port->handle->hdmatx);
    status = release_dma(&dma_rx, &rx_dma_owned, &port->handle->hdmarx);
    if (result == NX_OK) result = status;
    if (result != NX_OK) return result;
    if (pins_owned) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_SET);
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0 | GPIO_PIN_1);
        pins_owned = false;
    }
    if (clock_owned) {
        __HAL_RCC_SPI1_CLK_DISABLE();
        clock_owned = false;
    }
    manager = NULL;
    bound_handle = NULL;
    return NX_OK;
}
