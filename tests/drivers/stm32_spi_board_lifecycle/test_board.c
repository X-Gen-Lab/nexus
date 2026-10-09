/* Production Board resource callbacks with explicit release faults. Results
 * qualify ownership/retry behavior only: physical=false throughout. */
#include "stm32_spi_resource.h"
#include "hal/resource/nx_isr_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

SPI_TypeDef model_spi;
DMA_Stream_TypeDef model_dma_tx, model_dma_rx;
GPIO_TypeDef model_gpio_a, model_gpio_b;
static bool clocks[4], pins_a, pins_b, manager_present = true;
static unsigned clock_enables, pin_releases, dma_init_calls[2],
    dma_release_calls[2];
static unsigned dma_callbacks, spi_callbacks;
static int fail_init = -1, fail_release = -1, fail_connect = -1,
           fail_disconnect = -1;
static HAL_StatusTypeDef release_error = HAL_ERROR;
typedef struct {
    nx_isr_func_t callback;
    void* context;
    bool connected, external;
    unsigned connects, disconnects, clears;
} irq_entry_t;
static irq_entry_t irqs[64];
static unsigned dma_index(DMA_HandleTypeDef* dma) {
    assert(dma && dma->Parent);
    assert(dma->Instance == DMA2_Stream3 || dma->Instance == DMA2_Stream0);
    return dma->Instance == DMA2_Stream3 ? 0u : 1u;
}
void model_clock_enable(unsigned clock) {
    assert(clock < 4);
    clocks[clock] = true;
    ++clock_enables;
}
void model_clock_disable(unsigned clock) {
    assert(clock == 2);
    clocks[clock] = false;
}
void HAL_GPIO_WritePin(GPIO_TypeDef* gpio, uint16_t pins, GPIO_PinState value) {
    assert(gpio == GPIOB && value == GPIO_PIN_SET);
    assert(pins == (GPIO_PIN_0 | GPIO_PIN_1));
    gpio->value |= pins;
}
void HAL_GPIO_Init(GPIO_TypeDef* gpio, GPIO_InitTypeDef* config) {
    assert(config);
    if (gpio == GPIOA) {
        assert(config->Pin == (GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7));
        assert(config->Mode == GPIO_MODE_AF_PP &&
               config->Alternate == GPIO_AF5_SPI1);
        pins_a = true;
    } else {
        assert(gpio == GPIOB && config->Pin == (GPIO_PIN_0 | GPIO_PIN_1));
        assert((gpio->value & config->Pin) == config->Pin);
        pins_b = true;
    }
}
void HAL_GPIO_DeInit(GPIO_TypeDef* gpio, uint32_t pins) {
    if (gpio == GPIOA) {
        assert(pins_a && pins == (GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7));
        pins_a = false;
    } else {
        assert(gpio == GPIOB && pins_b && pins == (GPIO_PIN_0 | GPIO_PIN_1));
        pins_b = false;
    }
    ++pin_releases;
}
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef* dma) {
    unsigned index = dma_index(dma);
    ++dma_init_calls[index];
    /* Even a failed initialization can have written the stream. */
    dma->Instance->live = true;
    return fail_init == (int)index ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_DMA_DeInit(DMA_HandleTypeDef* dma) {
    unsigned index = dma_index(dma);
    assert(dma->Instance->live);
    assert(!irqs[DMA2_Stream3_IRQn].connected ||
           irqs[DMA2_Stream3_IRQn].external);
    assert(!irqs[DMA2_Stream0_IRQn].connected ||
           irqs[DMA2_Stream0_IRQn].external);
    assert(!irqs[SPI1_IRQn].connected || irqs[SPI1_IRQn].external);
    ++dma_release_calls[index];
    if (fail_release == (int)index)
        return release_error;
    dma->Instance->live = false;
    return HAL_OK;
}
void HAL_DMA_IRQHandler(DMA_HandleTypeDef* dma) {
    (void)dma_index(dma);
    assert(dma->Instance->live);
    ++dma_callbacks;
}
void HAL_SPI_IRQHandler(SPI_HandleTypeDef* spi) {
    assert(spi && spi->Instance == SPI1);
    ++spi_callbacks;
}
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) {
    assert(irq >= 0 && irq < 64);
    assert(!irqs[irq].external);
    ++irqs[irq].clears;
}
uint32_t HAL_RCC_GetPCLK2Freq(void) {
    return 84000000u;
}
static nx_status_t connect_irq(nx_isr_manager_t* self, uint32_t irq,
                               nx_isr_func_t callback, void* context,
                               uint8_t priority) {
    assert(self && irq < 64 && callback && context && priority == 5);
    irq_entry_t* entry = &irqs[irq];
    ++entry->connects;
    if (entry->external || fail_connect == (int)irq)
        return NX_ERR_BUSY;
    assert(!entry->connected);
    entry->callback = callback;
    entry->context = context;
    entry->connected = true;
    return NX_OK;
}
static nx_status_t disconnect_irq(nx_isr_manager_t* self, uint32_t irq) {
    assert(self && irq < 64 && irqs[irq].connected && !irqs[irq].external);
    irq_entry_t* entry = &irqs[irq];
    ++entry->disconnects;
    if (fail_disconnect == (int)irq)
        return NX_ERR_BUSY;
    entry->connected = false;
    entry->callback = NULL;
    entry->context = NULL;
    return NX_OK;
}
static nx_isr_manager_t manager = {.connect = connect_irq,
                                   .disconnect = disconnect_irq};
nx_isr_manager_t* nx_isr_manager_get(void) {
    return manager_present ? &manager : NULL;
}
static SPI_HandleTypeDef handle = {.Instance = SPI1};
static stm32_spi_board_port_t port = {.handle = &handle,
                                      .instance = 1,
                                      .dma_tx_enabled = true,
                                      .dma_rx_enabled = true};
static void released(void) {
    assert(!handle.hdmatx && !handle.hdmarx);
    assert(!model_dma_tx.live && !model_dma_rx.live && !pins_a && !pins_b &&
           !clocks[2]);
    assert(pin_releases == 2);
    assert(stm32_spi_board_release(&port) == NX_OK && pin_releases == 2);
}
static void callback(uint32_t irq) {
    assert(irqs[irq].connected);
    irqs[irq].callback(irqs[irq].context);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const char* scenario = argv[1];
    if (!strcmp(scenario, "unsupported-preflight")) {
        stm32_spi_board_port_t wrong = port;
        wrong.instance = 2;
        assert(stm32_spi_board_prepare(&wrong) == NX_ERR_NOT_SUPPORTED);
        assert(stm32_spi_board_prepare(NULL) == NX_ERR_NOT_SUPPORTED);
        assert(stm32_spi_board_release(&wrong) == NX_ERR_NOT_SUPPORTED);
        assert(!clock_enables && !dma_init_calls[0] && !pin_releases);
    } else if (!strcmp(scenario, "manager-preflight")) {
        manager_present = false;
        assert(stm32_spi_board_prepare(&port) == NX_ERR_NO_RESOURCE);
        assert(!clock_enables && !dma_init_calls[0]);
        manager_present = true;
        manager.disconnect = NULL;
        assert(stm32_spi_board_prepare(&port) == NX_ERR_NOT_SUPPORTED);
        assert(!clock_enables && !dma_init_calls[0]);
    } else if (!strcmp(scenario, "polling-idempotent")) {
        port.dma_tx_enabled = port.dma_rx_enabled = false;
        assert(stm32_spi_board_release(&port) == NX_OK && !clock_enables);
        assert(stm32_spi_board_prepare(&port) == NX_OK);
        assert(clocks[2] && pins_a && pins_b && !dma_init_calls[0]);
        unsigned before = clock_enables;
        assert(stm32_spi_board_prepare(&port) == NX_ERR_BUSY &&
               before == clock_enables);
        assert(stm32_spi_board_release(&port) == NX_OK);
        released();
    } else if (!strcmp(scenario, "irq-disconnect-retry")) {
        assert(stm32_spi_board_prepare(&port) == NX_OK);
        DMA_HandleTypeDef* tx = handle.hdmatx;
        DMA_HandleTypeDef* rx = handle.hdmarx;
        fail_disconnect = DMA2_Stream3_IRQn;
        assert(stm32_spi_board_release(&port) == NX_ERR_BUSY);
        assert(handle.hdmatx == tx && handle.hdmarx == rx &&
               model_dma_tx.live && model_dma_rx.live);
        assert(!dma_release_calls[0] && !dma_release_calls[1] &&
               !pin_releases && clocks[2]);
        assert(irqs[DMA2_Stream3_IRQn].connected &&
               !irqs[DMA2_Stream0_IRQn].connected &&
               !irqs[SPI1_IRQn].connected);
        callback(DMA2_Stream3_IRQn);
        assert(dma_callbacks == 1);
        fail_disconnect = -1;
        assert(stm32_spi_board_release(&port) == NX_OK);
        assert(irqs[DMA2_Stream3_IRQn].disconnects == 2 &&
               irqs[DMA2_Stream0_IRQn].disconnects == 1 &&
               irqs[SPI1_IRQn].disconnects == 1);
        assert(dma_release_calls[0] == 1 && dma_release_calls[1] == 1);
        released();
    } else if (!strcmp(scenario, "dma-disconnect-retry")) {
        assert(stm32_spi_board_prepare(&port) == NX_OK);
        DMA_HandleTypeDef* rx = handle.hdmarx;
        fail_release = 1;
        release_error = HAL_BUSY;
        assert(stm32_spi_board_release(&port) == NX_ERR_BUSY);
        assert(!handle.hdmatx && handle.hdmarx == rx && !model_dma_tx.live &&
               model_dma_rx.live);
        assert(rx->Instance == DMA2_Stream0 && rx->Parent == &handle &&
               !pin_releases && clocks[2]);
        fail_release = -1;
        assert(stm32_spi_board_release(&port) == NX_OK);
        assert(dma_release_calls[0] == 1 && dma_release_calls[1] == 2);
        released();
    } else if (!strcmp(scenario, "tx-init-partial")) {
        fail_init = 0;
        fail_release = 0;
        assert(stm32_spi_board_prepare(&port) == NX_ERR_DMA_CONFIG);
        assert(dma_init_calls[0] == 1 && !dma_init_calls[1] && handle.hdmatx &&
               !handle.hdmarx);
        assert(stm32_spi_board_release(&port) == NX_ERR_IO);
        assert(handle.hdmatx && model_dma_tx.live && !pin_releases);
        assert(stm32_spi_board_prepare(&port) == NX_ERR_BUSY);
        fail_release = -1;
        assert(stm32_spi_board_release(&port) == NX_OK);
        assert(dma_release_calls[0] == 2 && !dma_release_calls[1]);
        released();
    } else if (!strcmp(scenario, "rx-init-partial")) {
        fail_init = 1;
        assert(stm32_spi_board_prepare(&port) == NX_ERR_DMA_CONFIG);
        assert(handle.hdmatx && handle.hdmarx && model_dma_tx.live &&
               model_dma_rx.live);
        assert(!irqs[DMA2_Stream3_IRQn].connects && !pin_releases);
        assert(stm32_spi_board_release(&port) == NX_OK);
        assert(dma_release_calls[0] == 1 && dma_release_calls[1] == 1);
        released();
    } else if (!strcmp(scenario, "connect-partial")) {
        fail_connect = DMA2_Stream0_IRQn;
        assert(stm32_spi_board_prepare(&port) == NX_ERR_BUSY);
        assert(irqs[DMA2_Stream3_IRQn].connected &&
               !irqs[DMA2_Stream0_IRQn].connected && !irqs[SPI1_IRQn].connects);
        fail_disconnect = DMA2_Stream3_IRQn;
        assert(stm32_spi_board_release(&port) == NX_ERR_BUSY && handle.hdmatx &&
               handle.hdmarx);
        callback(DMA2_Stream3_IRQn);
        fail_disconnect = -1;
        assert(stm32_spi_board_release(&port) == NX_OK);
        assert(!irqs[DMA2_Stream0_IRQn].disconnects &&
               !irqs[DMA2_Stream0_IRQn].clears && !irqs[SPI1_IRQn].clears);
        released();
    } else if (!strcmp(scenario, "external-irq-owner")) {
        irqs[DMA2_Stream0_IRQn].external = irqs[DMA2_Stream0_IRQn].connected =
            true;
        assert(stm32_spi_board_prepare(&port) == NX_ERR_BUSY);
        assert(stm32_spi_board_release(&port) == NX_OK);
        assert(irqs[DMA2_Stream0_IRQn].connected &&
               !irqs[DMA2_Stream0_IRQn].disconnects &&
               !irqs[DMA2_Stream0_IRQn].clears);
        released();
    } else if (!strcmp(scenario, "handle-owner")) {
        assert(stm32_spi_board_prepare(&port) == NX_OK);
        SPI_HandleTypeDef other = {.Instance = SPI1};
        stm32_spi_board_port_t other_port = port;
        other_port.handle = &other;
        assert(stm32_spi_board_release(&other_port) == NX_ERR_RESOURCE_BUSY);
        assert(!pin_releases && !dma_release_calls[0] && handle.hdmatx &&
               handle.hdmarx);
        callback(SPI1_IRQn);
        assert(spi_callbacks == 1);
        assert(stm32_spi_board_release(&port) == NX_OK);
        released();
        other.hdmatx = (DMA_HandleTypeDef*)&other;
        unsigned before = clock_enables;
        assert(stm32_spi_board_prepare(&other_port) == NX_ERR_RESOURCE_BUSY &&
               before == clock_enables);
    } else {
        assert(!"unknown scenario");
    }
    printf("Production Discovery SPI Board ownership model: %s passed; physical=false\n",
           scenario);
    return 0;
}
