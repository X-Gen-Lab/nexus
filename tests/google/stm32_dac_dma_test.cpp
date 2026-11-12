/**
 * \file            stm32_dac_dma_test.cpp
 * \brief           Real DAC DMA ownership, pipeline and withdrawal contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_dac_dma.h"
}
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <vector>

namespace {
struct Store {
    volatile uint32_t* address;
    uint32_t value;
};
std::vector<Store> stores;
nx_time_us_t clock_us;
bool privileged;
bool in_isr;
bool masked;
bool hold_stream;
bool hold_address;
bool hold_dma_clock;
bool hold_priority;
bool hold_pending;
bool hold_dac_request;
bool hold_update;
bool reject_stream_enable;
bool reject_timer_enable;
bool hold_trigger;
bool hold_interrupt;
bool hold_underrun;
bool foreign_stream_on_clock;
bool update_while_clearing;
bool expire_on_dma_enable;
bool delay_dma_ack;
bool refuse_dac_clock_enable;
bool refuse_dac_clock_disable;
bool suppress_hardware_trigger;
bool expire_on_recovery_trigger;
bool dac_clock_withdrawn;
bool changed_tsel_while_enabled;
bool lose_time_on_dma_enable;
bool lose_time_in_recovery_wait;
bool clock_lost;
unsigned recovery_reads;
unsigned wake_count;
uint32_t reload_count[2];
size_t next_frame[2];
bool request_pending[2];
bool underrun_blocked[2];
uint8_t restart_steps[2];
enum {
    DMA_OFF = 1U,
    CLOCK_OFF = 2U,
    DAC_CONFIGURED = 4U,
    TRIGGER_CONFIGURED = 8U,
    DMA_CONFIGURED = 16U
};
struct CpuModel {
    bool enabled[82];
    bool pending[82];
    bool active[82];
    uint8_t priority[82];
    uint8_t group;
};
CpuModel cpu_model{};

void holding(unsigned index, uint32_t code) {
    volatile uint32_t* data = index == 0U ? &DAC->DHR12R1 : &DAC->DHR12R2;
    volatile uint32_t* output = index == 0U ? &DAC->DOR1 : &DAC->DOR2;
    *data = code;
    if ((DAC->CR & (DAC_CR_TEN1 << (16U * index))) == 0U) {
        *output = code;
    }
}
DMA_Stream_TypeDef* stream_for(uint8_t channels) {
    return channels == 2U ? DMA1_Stream6 : DMA1_Stream5;
}
unsigned stream_shift(uint8_t channels) {
    return channels == 2U ? 16U : 6U;
}
uint32_t stream_tc(uint8_t channels) {
    return 0x20U << stream_shift(channels);
}
IRQn_Type stream_irq(uint8_t channels) {
    return channels == 2U ? DMA1_Stream6_IRQn : DMA1_Stream5_IRQn;
}
uint32_t request_mask(uint8_t channels) {
    return channels == 2U ? DAC_CR_DMAEN2 : DAC_CR_DMAEN1;
}
uint32_t underrun_mask(uint8_t channels) {
    return channels == 2U ? DAC_SR_DMAUDR2 : DAC_SR_DMAUDR1;
}
/** \brief A pending peripheral request is serviced on valid reenable too. */
void service_pending(unsigned index) {
    auto* stream = index == 0U ? DMA1_Stream5 : DMA1_Stream6;
    const uint32_t shift = 16U * index;
    if (delay_dma_ack || !request_pending[index] || underrun_blocked[index] ||
        (RCC->APB1ENR & RCC_APB1ENR_DACEN) == 0U ||
        (RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN) == 0U ||
        (DAC->CR & (DAC_CR_EN1 << shift)) == 0U ||
        (DAC->CR & (DAC_CR_DMAEN1 << shift)) == 0U ||
        (stream->CR & DMA_SxCR_EN) == 0U || stream->NDTR == 0U) {
        return;
    }
    const size_t width =
        size_t{1U} << ((stream->CR & DMA_SxCR_MSIZE) >> DMA_SxCR_MSIZE_Pos);
    uint32_t frame = 0U;
    std::memcpy(&frame,
                reinterpret_cast<const uint8_t*>(stream->M0AR) +
                    next_frame[index] * width,
                width);
    request_pending[index] = false;
    nx_dac_model_write(reinterpret_cast<volatile uint32_t*>(stream->PAR),
                       frame);
    ++next_frame[index];
    --stream->NDTR;
    if (stream->NDTR == 0U) {
        DMA1->HISR |= stream_tc(static_cast<uint8_t>(index + 1U));
        if ((stream->CR & DMA_SxCR_CIRC) != 0U) {
            stream->NDTR = reload_count[index];
            next_frame[index] = 0U;
        } else {
            stream->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        }
    }
}
/** \brief Only the documented full restart exits an underrun-blocked domain.
 */
void restart_if_complete(unsigned index) {
    constexpr uint8_t complete = DMA_OFF | CLOCK_OFF | DAC_CONFIGURED |
                                 TRIGGER_CONFIGURED | DMA_CONFIGURED;
    const uint32_t shift = 16U * index;
    if (restart_steps[index] == complete &&
        (DAC->CR & (DAC_CR_EN1 << shift)) != 0U &&
        (DAC->CR & (DAC_CR_DMAEN1 << shift)) != 0U &&
        (RCC->APB1ENR & RCC_APB1ENR_DACEN) != 0U) {
        request_pending[index] = underrun_blocked[index] = false;
        restart_steps[index] = 0U;
    }
}
/** \brief A second hardware event before acknowledge underruns, never queues.
 */
void hardware_trigger(TIM_TypeDef* timer) {
    if (suppress_hardware_trigger || (RCC->APB1ENR & RCC_APB1ENR_DACEN) == 0U) {
        return;
    }
    if (timer == nullptr && ((EXTI->EMR & EXTI->RTSR & (1U << 9U)) == 0U ||
                             ((SYSCFG->EXTICR[2] >> 4U) & 15U) != 1U))
        return;
    for (unsigned index = 0U; index < 2U; ++index) {
        const uint32_t shift = 16U * index;
        const uint32_t source = timer == TIM6 ? 0U
                                : timer == TIM7
                                    ? DAC_CR_TSEL1_1
                                    : DAC_CR_TSEL1_2 | DAC_CR_TSEL1_1;
        if ((DAC->CR & (DAC_CR_EN1 << shift)) == 0U ||
            (DAC->CR & (DAC_CR_TEN1 << shift)) == 0U ||
            ((DAC->CR >> shift) & DAC_CR_TSEL1) != source) {
            continue;
        }
        if (index == 0U) {
            DAC->DOR1 = DAC->DHR12R1;
        } else {
            DAC->DOR2 = DAC->DHR12R2;
        }
        if ((DAC->CR & (DAC_CR_DMAEN1 << shift)) != 0U) {
            if (expire_on_recovery_trigger && timer != nullptr &&
                (timer->CR1 & TIM_CR1_CEN) == 0U) {
                clock_us = 10000U;
            }
            if (request_pending[index]) {
                DAC->SR |= static_cast<uint32_t>(DAC_SR_DMAUDR1) << shift;
                underrun_blocked[index] = true;
                restart_steps[index] = 0U;
            } else {
                request_pending[index] = true;
            }
            service_pending(index);
        }
    }
}
nx_result_t wake(void*) {
    ++wake_count;
    return NX_SUCCESS;
}
} /* namespace */

extern "C" void nx_dac_model_write(volatile uint32_t* reg, uint32_t value) {
    stores.push_back({reg, value});
    if (reg == &EXTI->PR) {
        *reg &= ~value;
        return;
    }
    if (reinterpret_cast<uintptr_t>(reg) >= reinterpret_cast<uintptr_t>(DAC) &&
        reinterpret_cast<uintptr_t>(reg) <
            reinterpret_cast<uintptr_t>(DAC) + sizeof(*DAC) &&
        (RCC->APB1ENR & RCC_APB1ENR_DACEN) == 0U) {
        return;
    }
    if (reg == &DMA1->HIFCR) {
        DMA1->HISR &= ~value;
        *reg = value;
        return;
    }
    if (reg == &DAC->SR) {
        if (!hold_underrun) {
            *reg &= ~value;
        }
        return;
    }
    if (reg == &TIM6->SR || reg == &TIM7->SR) {
        if (!hold_update) {
            *reg &= value;
        }
        if (update_while_clearing &&
            ((reg == &TIM6->SR ? TIM6 : TIM7)->CR1 & TIM_CR1_CEN) != 0U) {
            *reg |= TIM_SR_UIF;
        }
        return;
    }
    if (reg == &DMA1_Stream5->CR || reg == &DMA1_Stream6->CR) {
        if (lose_time_on_dma_enable && (value & DMA_SxCR_EN) != 0U)
            clock_lost = true;
        if (expire_on_dma_enable && (value & DMA_SxCR_EN) != 0U) {
            clock_us = 10000U;
        }
        if (hold_interrupt) {
            value |=
                *reg & static_cast<uint32_t>(DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                                             DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
        }
        if (hold_stream && (*reg & DMA_SxCR_EN) != 0U) {
            value |= DMA_SxCR_EN;
        }
        if (reject_stream_enable) {
            value &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        }
    }
    if (hold_address &&
        (reg == &DMA1_Stream5->M0AR || reg == &DMA1_Stream6->M0AR) &&
        value == 0U) {
        value = *reg;
    }
    if (hold_dma_clock && reg == &RCC->AHB1ENR) {
        value |= RCC->AHB1ENR & static_cast<uint32_t>(RCC_AHB1ENR_DMA1EN);
    }
    if (hold_dac_request && reg == &DAC->CR) {
        value |= DAC->CR & static_cast<uint32_t>(DAC_CR_DMAEN1 | DAC_CR_DMAEN2);
    }
    if (hold_trigger && reg == &DAC->CR) {
        value |= DAC->CR & static_cast<uint32_t>(DAC_CR_TEN1 | DAC_CR_TEN2);
    }
    if (foreign_stream_on_clock && reg == &RCC->AHB1ENR &&
        (value & RCC_AHB1ENR_DMA1EN) != 0U) {
        DMA1_Stream5->CR |= DMA_SxCR_EN;
    }
    if (reject_timer_enable && (reg == &TIM6->CR1 || reg == &TIM7->CR1)) {
        value &= ~static_cast<uint32_t>(TIM_CR1_CEN);
    }
    if (reg == &TIM6->EGR || reg == &TIM7->EGR) {
        auto* timer = reg == &TIM6->EGR ? TIM6 : TIM7;
        timer->SR |= TIM_SR_UIF;
        for (unsigned index = 0U; index < 2U; ++index) {
            const uint32_t shift = 16U * index;
            if ((restart_steps[index] & CLOCK_OFF) != 0U &&
                (DAC->CR & (DAC_CR_TEN1 << shift)) == 0U) {
                restart_steps[index] |= TRIGGER_CONFIGURED;
            }
        }
        if ((timer->CR2 & TIM_CR2_MMS) == 0U ||
            (timer->CR2 & TIM_CR2_MMS) == TIM_CR2_MMS_1) {
            hardware_trigger(timer);
        }
    }
    if (reg == &RCC->APB1ENR) {
        if (refuse_dac_clock_disable) {
            value |= RCC->APB1ENR & static_cast<uint32_t>(RCC_APB1ENR_DACEN);
        }
        const bool falling = (RCC->APB1ENR & RCC_APB1ENR_DACEN) != 0U &&
                             (value & RCC_APB1ENR_DACEN) == 0U;
        if (falling) {
            dac_clock_withdrawn = true;
            for (unsigned index = 0U; index < 2U; ++index) {
                if (underrun_blocked[index] &&
                    (restart_steps[index] & DMA_OFF) != 0U) {
                    restart_steps[index] |= CLOCK_OFF;
                }
            }
        }
        if (refuse_dac_clock_enable && dac_clock_withdrawn) {
            value &= ~static_cast<uint32_t>(RCC_APB1ENR_DACEN);
        }
    }
    if (reg == &DAC->CR) {
        for (unsigned index = 0U; index < 2U; ++index) {
            const uint32_t shift = 16U * index;
            if ((DAC->CR & (DAC_CR_EN1 << shift)) != 0U &&
                ((DAC->CR ^ value) & (DAC_CR_TSEL1 << shift)) != 0U)
                changed_tsel_while_enabled = true;
            if (lose_time_in_recovery_wait &&
                ((DAC->CR >> shift) & DAC_CR_TSEL1) ==
                    (DAC_CR_TSEL1_2 | DAC_CR_TSEL1_1) &&
                ((value >> shift) & DAC_CR_TSEL1) !=
                    (DAC_CR_TSEL1_2 | DAC_CR_TSEL1_1))
                recovery_reads = 2U;
            if (underrun_blocked[index] &&
                (DAC->CR & (DAC_CR_DMAEN1 << shift)) != 0U &&
                (value & (DAC_CR_DMAEN1 << shift)) == 0U) {
                restart_steps[index] |= DMA_OFF;
            }
            if ((restart_steps[index] & CLOCK_OFF) != 0U &&
                (value & (DAC_CR_EN1 << shift)) == 0U) {
                restart_steps[index] |= DAC_CONFIGURED;
                if ((value & (DAC_CR_TEN1 << shift)) == 0U &&
                    ((value >> shift) & DAC_CR_TSEL1) ==
                        (DAC_CR_TSEL1_2 | DAC_CR_TSEL1_1))
                    restart_steps[index] |= TRIGGER_CONFIGURED;
            }
        }
    }
    if (reg == &DMA1_Stream5->CR || reg == &DMA1_Stream6->CR) {
        unsigned index = reg == &DMA1_Stream5->CR ? 0U : 1U;
        if ((*reg & DMA_SxCR_EN) == 0U && (value & DMA_SxCR_EN) != 0U) {
            auto* stream = index == 0U ? DMA1_Stream5 : DMA1_Stream6;
            reload_count[index] = stream->NDTR;
            next_frame[index] = 0U;
            if ((restart_steps[index] & CLOCK_OFF) != 0U &&
                stream->NDTR != 0U && stream->PAR != 0U && stream->M0AR != 0U &&
                ((value & DMA_SxCR_CHSEL) >> DMA_SxCR_CHSEL_Pos) == 7U) {
                restart_steps[index] |= DMA_CONFIGURED;
            }
        }
    }
    *reg = value;
    if (reg == &DAC->DHR12R1 || reg == &DAC->DHR12L1 || reg == &DAC->DHR8R1) {
        holding(0U, reg == &DAC->DHR12L1  ? value >> 4U
                    : reg == &DAC->DHR8R1 ? value << 4U
                                          : value);
    } else if (reg == &DAC->DHR12R2 || reg == &DAC->DHR12L2 ||
               reg == &DAC->DHR8R2) {
        holding(1U, reg == &DAC->DHR12L2  ? value >> 4U
                    : reg == &DAC->DHR8R2 ? value << 4U
                                          : value);
    } else if (reg == &DAC->DHR12RD || reg == &DAC->DHR12LD) {
        const unsigned shift = reg == &DAC->DHR12LD ? 4U : 0U;
        holding(0U, (value >> shift) & 0xfffU);
        holding(1U, (value >> (16U + shift)) & 0xfffU);
    } else if (reg == &DAC->DHR8RD) {
        holding(0U, (value & 0xffU) << 4U);
        holding(1U, ((value >> 8U) & 0xffU) << 4U);
    }
    for (unsigned index = 0U; index < 2U; ++index) {
        restart_if_complete(index);
        service_pending(index);
    }
}
extern "C" void nx_dac_dma_model_enable(IRQn_Type irq) {
    cpu_model.enabled[static_cast<unsigned>(irq)] = true;
}
extern "C" void nx_dac_dma_model_disable(IRQn_Type irq) {
    cpu_model.enabled[static_cast<unsigned>(irq)] = false;
}
extern "C" void nx_dac_dma_model_pending(IRQn_Type irq) {
    if (!hold_pending) {
        cpu_model.pending[static_cast<unsigned>(irq)] = false;
    }
}
extern "C" void nx_dac_dma_model_priority(IRQn_Type irq, uint32_t priority) {
    if (!hold_priority) {
        cpu_model.priority[static_cast<unsigned>(irq)] =
            static_cast<uint8_t>(priority);
    }
}
extern "C" uint32_t nx_dac_dma_model_get_priority(IRQn_Type irq) {
    return cpu_model.priority[static_cast<unsigned>(irq)];
}
extern "C" uint32_t nx_dac_dma_model_get_enable(IRQn_Type irq) {
    return cpu_model.enabled[static_cast<unsigned>(irq)] ? 1U : 0U;
}
extern "C" uint32_t nx_dac_dma_model_get_pending(IRQn_Type irq) {
    return cpu_model.pending[static_cast<unsigned>(irq)] ? 1U : 0U;
}
extern "C" uint32_t nx_dac_dma_model_get_active(IRQn_Type irq) {
    return cpu_model.active[static_cast<unsigned>(irq)] ? 1U : 0U;
}
extern "C" uint32_t nx_dac_dma_model_get_group(void) {
    return cpu_model.group;
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return in_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return masked;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous{masked ? 1U : 0U};
    masked = true;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    masked = previous.value != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    if (recovery_reads != 0U && --recovery_reads == 0U)
        clock_lost = true;
    if (clock_lost)
        return NX_DEADLINE_NEVER;
    return clock_us++;
}

class DacDma : public testing::Test {
  protected:
    void* peripheral = MAP_FAILED;
    void* memory = MAP_FAILED;
    const nx_dma_memory_region_t region{0x20000000U, 65536U,
                                        NX_DMA_MEMORY_READ};
    nx_stm32_dac_dma_config_t config{};
    nx_stm32_dac_dma_state_t state{};
    nx_dac_tx_request_t request{};
    nx_dac_port_t control{&nx_stm32_dac_dma_ops, &state};
    nx_dac_tx_port_t tx{&nx_stm32_dac_dma_tx_ops, &state};
    uint8_t* data = reinterpret_cast<uint8_t*>(0x20001000U);
    size_t width = 2U;

    void SetUp() override {
        peripheral =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        memory = mmap(reinterpret_cast<void*>(0x20000000U), 65536U,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(peripheral, reinterpret_cast<void*>(0x40000000U));
        ASSERT_EQ(memory, reinterpret_cast<void*>(0x20000000U));
        cpu_model = {};
        stores.clear();
        privileged = true;
        in_isr = masked = hold_stream = hold_address = hold_dma_clock =
            hold_priority = hold_pending = hold_dac_request = hold_update =
                reject_stream_enable = reject_timer_enable = false;
        clock_us = 100U;
        wake_count = 0U;
        for (unsigned index = 0U; index < 2U; ++index) {
            next_frame[index] = reload_count[index] = 0U;
            request_pending[index] = underrun_blocked[index] = false;
            restart_steps[index] = 0U;
        }
        delay_dma_ack = refuse_dac_clock_enable = dac_clock_withdrawn = false;
        changed_tsel_while_enabled = false;
        lose_time_on_dma_enable = lose_time_in_recovery_wait = false;
        clock_lost = false;
        recovery_reads = 0U;
        refuse_dac_clock_disable = suppress_hardware_trigger =
            expire_on_recovery_trigger = false;
        hold_trigger = hold_interrupt = hold_underrun =
            foreign_stream_on_clock = false;
        update_while_clearing = expire_on_dma_enable = false;
        RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN;
        config.dac.registers = DAC;
        config.dac.channels = 1U;
        config.dac.format = NX_DAC_FORMAT_U12_RIGHT;
        config.dac.mode = NX_STM32_DAC_TRIGGERED;
        config.dac.trigger = NX_STM32_DAC_TRIGGER_TIM6;
        config.dac.output_buffer = true;
        config.regions = &region;
        config.region_count = 1U;
        config.priority = 5U;
        nx_request_initialize(&request.base);
        const uint16_t codes[] = {0x123U, 0x456U};
        std::memcpy(data, codes, sizeof(codes));
    }
    void TearDown() override {
        hold_stream = hold_address = hold_dma_clock = hold_pending =
            hold_dac_request = hold_priority = hold_update = false;
        reject_stream_enable = reject_timer_enable = false;
        hold_trigger = hold_interrupt = hold_underrun =
            foreign_stream_on_clock = false;
        update_while_clearing = expire_on_dma_enable = false;
        delay_dma_ack = refuse_dac_clock_enable = false;
        refuse_dac_clock_disable = suppress_hardware_trigger =
            expire_on_recovery_trigger = false;
        privileged = true;
        in_isr = masked = false;
        clock_us = 100U;
        if (peripheral != MAP_FAILED) {
            for (bool& active : cpu_model.active) {
                active = false;
            }
            (void)nx_stm32_dac_dma_close(&state);
        }
        if (peripheral != MAP_FAILED) {
            EXPECT_EQ(munmap(peripheral, 0x80000U), 0);
        }
        if (memory != MAP_FAILED) {
            EXPECT_EQ(munmap(memory, 65536U), 0);
        }
    }
    void prepare() {
        ASSERT_EQ(nx_stm32_dac_dma_prepare(&state, &config), NX_SUCCESS);
    }
    void start(size_t count = 2U, nx_time_us_t deadline = 1000000U) {
        ASSERT_EQ(
            nx_dac_tx_prepare(&request, data, count * width, 1000U, deadline),
            NX_SUCCESS);
        ASSERT_EQ(nx_dac_tx_port_submit(&tx, &request), NX_SUCCESS);
    }
    void irq() {
        in_isr = true;
        nx_stm32_dac_dma_irq(&state);
        in_isr = false;
    }
    void pulse(bool deliver_irq = true) {
        TIM_TypeDef* timer =
            config.dac.trigger == NX_STM32_DAC_TRIGGER_TIM6 ? TIM6 : TIM7;
        if ((timer->CR1 & TIM_CR1_CEN) == 0U) {
            return;
        }
        clock_us += 1000U;
        timer->SR |= TIM_SR_UIF;
        hardware_trigger(timer);
        clock_us += 4U;
        if (deliver_irq &&
            ((DMA1->HISR & stream_tc(config.dac.channels)) != 0U ||
             (DAC->SR & underrun_mask(config.dac.channels)) != 0U)) {
            irq();
        }
    }
    void result(nx_result_t expected) {
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
        nx_result_t observed;
        size_t transferred;
        ASSERT_EQ(nx_request_result(&request.base, &observed, &transferred),
                  NX_SUCCESS);
        EXPECT_EQ(observed, expected);
        EXPECT_LE(transferred, request.length);
        EXPECT_EQ(stream_for(config.dac.channels)->M0AR, 0U);
    }
};

TEST_F(DacDma, PreparationOwnsOnlyItsSelectedStaticRoute) {
    prepare();
    EXPECT_TRUE(state.initialized);
    EXPECT_NE(RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN, 0U);
    EXPECT_NE(NVIC_GetEnableIRQ(DMA1_Stream5_IRQn), 0U);
    EXPECT_NE(NVIC_GetEnableIRQ(TIM6_DAC_IRQn), 0U);
    EXPECT_EQ(NVIC_GetEnableIRQ(DMA1_Stream6_IRQn), 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
}
TEST_F(DacDma, FiniteCompletionWaitsForTheLastDigitalLatch) {
    prepare();
    start();
    pulse();
    EXPECT_EQ(DAC->DOR1, 0U);
    pulse();
    EXPECT_EQ(DAC->DOR1, 0x123U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    nx_dac_tx_port_service(&tx);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    pulse();
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
    EXPECT_EQ(DAC->DOR1, 0x456U);
    EXPECT_EQ(TIM6->CR1 & TIM_CR1_CEN, 0U);
}
TEST_F(DacDma, RepeatedLastCodeStillNeedsANewTimerUpdate) {
    std::memset(data, 0, 4U);
    prepare();
    start();
    pulse();
    pulse();
    clock_us += 2000U;
    nx_dac_tx_port_service(&tx);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    pulse();
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
}
TEST_F(DacDma, SingleFrameStillNeedsAConversionAfterMemoryCompletion) {
    prepare();
    start(1U);
    pulse();
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    pulse();
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
    EXPECT_EQ(DAC->DOR1, 0x123U);
}
TEST_F(DacDma, CircularPlaybackRetainsItsLoanAcrossWraps) {
    config.circular = true;
    prepare();
    start();
    pulse();
    pulse();
    pulse();
    pulse();
    nx_dac_tx_port_service(&tx);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(stream_for(1U)->CR & DMA_SxCR_TCIE, 0U);
    EXPECT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_CANCELLED);
}
TEST_F(DacDma, MisalignedAndUnreachableSourcesRejectWithoutStoresOrLoans) {
    prepare();
    const auto before = stores.size();
    ASSERT_EQ(nx_dac_tx_prepare(&request, data + 1U, 4U, 1000U, 10000U),
              NX_SUCCESS);
    EXPECT_EQ(nx_dac_tx_port_submit(&tx, &request), NX_ERROR_INVALID);
    ASSERT_EQ(nx_dac_tx_prepare(&request, reinterpret_cast<void*>(0x10000000U),
                                4U, 1000U, 10000U),
              NX_SUCCESS);
    EXPECT_NE(nx_dac_tx_port_submit(&tx, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(stores.size(), before);
}
TEST_F(DacDma, ExpiredAdmissionDoesNotEnableAnyEngine) {
    prepare();
    ASSERT_EQ(nx_dac_tx_prepare(&request, data, 4U, 1000U, 0U), NX_SUCCESS);
    const auto before = stores.size();
    EXPECT_EQ(nx_dac_tx_port_submit(&tx, &request), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(stores.size(), before);
}
TEST_F(DacDma, ActiveControllerRejectsASecondDescriptorAndCpuControls) {
    prepare();
    start();
    nx_dac_tx_request_t other{};
    nx_request_initialize(&other.base);
    ASSERT_EQ(nx_dac_tx_prepare(&other, data, 4U, 1000U, 10000U), NX_SUCCESS);
    EXPECT_EQ(nx_dac_tx_port_submit(&tx, &other), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&other.base), NX_REQUEST_READY);
    EXPECT_EQ(nx_dac_port_write(&control, 1U, 1U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_dac_port_write_pair(&control, 1U, 2U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_dac_port_start(&control, 1000U, 10000U), NX_ERROR_UNSUPPORTED);
}
TEST_F(DacDma, StartFailureAfterAdmissionIsASettledFailureNotRejection) {
    prepare();
    reject_stream_enable = true;
    start();
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_IO);
}
TEST_F(DacDma, TimeoutWithdrawsAnAcceptedLoanBeforePublication) {
    prepare();
    start(2U, 10000U);
    clock_us = 11000U;
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_TIMEOUT);
}
TEST_F(DacDma, UnderrunRequiresExplicitIdleRecoveryBeforeReuse) {
    prepare();
    start();
    DAC->SR |= DAC_SR_DMAUDR1;
    irq();
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_IO);
    EXPECT_EQ(nx_dac_port_stop(&control, NX_DEADLINE_NEVER), NX_SUCCESS);
    EXPECT_EQ(DAC->SR & DAC_SR_DMAUDR1, 0U);
    EXPECT_EQ(DAC->DOR1, 0U);
    nx_request_initialize(&request.base);
    start();
}
TEST_F(DacDma, LateFiniteCompletionCannotHideADacUnderrun) {
    prepare();
    start();
    pulse(false);
    pulse(false);
    pulse(false);
    pulse(false);
    irq();
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_IO);
}
TEST_F(DacDma, CancelGeneratedCompletionFlagIsNeverSuccessfulPlayback) {
    prepare();
    start();
    EXPECT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    DMA1->HISR |= stream_tc(1U);
    DAC->DOR1 = 0x456U;
    irq();
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_CANCELLED);
}
TEST_F(DacDma, UndrainedEngineRetainsTheBorrowAndClockUntilRetry) {
    prepare();
    start();
    hold_stream = true;
    EXPECT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    clock_us += 1000U;
    nx_dac_tx_port_service(&tx);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_stm32_dac_dma_close(&state), NX_ERROR_BUSY);
    EXPECT_NE(RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN, 0U);
    hold_stream = false;
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_CANCELLED);
}
TEST_F(DacDma, RetainedMemoryAddressPreventsEarlySettlement) {
    prepare();
    start();
    hold_address = true;
    EXPECT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    clock_us += 1000U;
    nx_dac_tx_port_service(&tx);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    hold_address = false;
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_CANCELLED);
}
TEST_F(DacDma, ForeignEnabledPendingAndActiveVectorsRejectBeforeTakingPins) {
    const unsigned vector = static_cast<unsigned>(TIM6_DAC_IRQn);
    for (bool* bank : {&cpu_model.enabled[vector], &cpu_model.pending[vector],
                       &cpu_model.active[vector]}) {
        *bank = true;
        EXPECT_EQ(nx_stm32_dac_dma_prepare(&state, &config), NX_ERROR_BUSY);
        EXPECT_EQ(DAC->CR, 0U);
        *bank = false;
    }
}
TEST_F(DacDma, PriorityWriteRefusalRetainsItsPartialResourceReceipt) {
    hold_priority = true;
    EXPECT_EQ(nx_stm32_dac_dma_prepare(&state, &config), NX_ERROR_IO);
    EXPECT_NE(state.dac.config, nullptr);
    hold_priority = false;
    EXPECT_EQ(nx_stm32_dac_dma_close(&state), NX_SUCCESS);
}
TEST_F(DacDma, ClockWithdrawalFailureKeepsTheSameReceiptRetryable) {
    prepare();
    hold_dma_clock = true;
    EXPECT_EQ(nx_stm32_dac_dma_close(&state), NX_ERROR_BUSY);
    EXPECT_NE(state.dac.config, nullptr);
    hold_dma_clock = false;
    EXPECT_EQ(nx_stm32_dac_dma_close(&state), NX_SUCCESS);
    EXPECT_EQ(state.dac.config, nullptr);
}
TEST_F(DacDma, ExistingClockAndSavedPrioritiesArePreservedByFullClose) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    cpu_model.priority[static_cast<unsigned>(DMA1_Stream5_IRQn)] = 11U;
    cpu_model.priority[static_cast<unsigned>(TIM6_DAC_IRQn)] = 12U;
    prepare();
    EXPECT_EQ(nx_stm32_dac_dma_close(&state), NX_SUCCESS);
    EXPECT_NE(RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN, 0U);
    EXPECT_EQ(NVIC_GetPriority(DMA1_Stream5_IRQn), 11U);
    EXPECT_EQ(NVIC_GetPriority(TIM6_DAC_IRQn), 12U);
}
TEST_F(DacDma, WakeAttachValidatesBothActualPublishersAndStopsBeforeDetach) {
    config.kernel_bound = true;
    prepare();
    const nx_irq_wake_t target{nullptr, wake, true};
    cpu_model.priority[static_cast<unsigned>(TIM6_DAC_IRQn)] = 1U;
    EXPECT_EQ(nx_dac_tx_port_attach_wake(&tx, &target, 5U),
              NX_ERROR_PERMISSION);
    cpu_model.priority[static_cast<unsigned>(TIM6_DAC_IRQn)] = 5U;
    EXPECT_EQ(nx_dac_tx_port_attach_wake(&tx, &target, 5U), NX_SUCCESS);
    start();
    pulse();
    pulse();
    EXPECT_GT(wake_count, 0U);
    EXPECT_EQ(nx_dac_tx_port_attach_wake(&tx, nullptr, 0U), NX_ERROR_BUSY);
    pulse();
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
    EXPECT_EQ(nx_dac_tx_port_attach_wake(&tx, nullptr, 0U), NX_SUCCESS);
}
TEST_F(DacDma, ContextRejectionPreservesIncomingMaskAndHasNoStores) {
    prepare();
    masked = true;
    ASSERT_EQ(nx_dac_tx_prepare(&request, data, 4U, 1000U, 10000U), NX_SUCCESS);
    const auto before = stores.size();
    EXPECT_EQ(nx_dac_tx_port_submit(&tx, &request), NX_ERROR_CONTEXT);
    EXPECT_TRUE(masked);
    EXPECT_EQ(stores.size(), before);
}
TEST_F(DacDma, TailSchedulingDeadlineAvoidsAnImplicitBusyWorker) {
    prepare();
    EXPECT_EQ(nx_dac_tx_port_next_service(&tx), NX_DEADLINE_NEVER);
    start();
    EXPECT_EQ(nx_dac_tx_port_next_service(&tx), request.base.deadline);
    pulse();
    pulse();
    const nx_time_us_t tail = nx_dac_tx_port_next_service(&tx);
    EXPECT_GT(tail, clock_us);
    EXPECT_LT(tail, request.base.deadline);
}

TEST_F(DacDma, EachFiniteTransferBeginsWithItsDeclaredIdleCode) {
    prepare();
    start();
    pulse();
    pulse();
    pulse();
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
    start();
    pulse();
    EXPECT_EQ(DAC->DOR1, config.dac.idle_codes[0]);
}
TEST_F(DacDma, InfoCannotOverwriteTheBorrowedDescriptor) {
    prepare();
    start();
    std::array<uint8_t, sizeof(request)> before{};
    std::memcpy(before.data(), &request, before.size());
    EXPECT_EQ(
        nx_dac_port_info(&control, reinterpret_cast<nx_dac_info_t*>(&request)),
        NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(before.data(), &request, before.size()), 0);
    std::memcpy(&request, before.data(), before.size());
}
TEST_F(DacDma, SnapshotCannotOverwriteImmutableDmaFrames) {
    prepare();
    start(32U);
    std::array<uint8_t, 64U> before{};
    std::memcpy(before.data(), data, before.size());
    EXPECT_EQ(
        nx_dac_port_state(&control, reinterpret_cast<nx_dac_state_t*>(data)),
        NX_ERROR_INVALID);
    EXPECT_EQ(std::memcmp(before.data(), data, before.size()), 0);
}
TEST_F(DacDma, DirectSubmissionRejectsOverlappingMetadataAndFrames) {
    prepare();
    auto* alias = reinterpret_cast<nx_dac_tx_request_t*>(data);
    nx_request_initialize(&alias->base);
    alias->data = alias;
    alias->length = sizeof(*alias);
    alias->trigger_hz = 1000U;
    alias->base.deadline = 10000U;
    const auto before = stores.size();
    EXPECT_EQ(nx_dac_tx_port_submit(&tx, alias), NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&alias->base), NX_REQUEST_READY);
    EXPECT_EQ(stores.size(), before);
}
TEST_F(DacDma, AnArmTimeoutBeforeDmaSetupReportsZeroTransferredBytes) {
    prepare();
    start(2U, clock_us + 2U);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_TIMEOUT);
    EXPECT_EQ(request.base.transferred, 0U);
}
TEST_F(DacDma, RefusedTriggerWithdrawalKeepsTheFiniteLoan) {
    prepare();
    start();
    pulse();
    pulse();
    hold_trigger = true;
    pulse();
    clock_us += 1000U;
    nx_dac_tx_port_service(&tx);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    hold_trigger = false;
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
}
TEST_F(DacDma, RefusedInterruptWithdrawalKeepsTheFiniteLoan) {
    prepare();
    start();
    pulse();
    pulse();
    hold_interrupt = true;
    pulse();
    nx_dac_tx_port_service(&tx);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    hold_interrupt = false;
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
}
TEST_F(DacDma, ExplicitUnderrunRecoveryVerifiesTheW1cReadback) {
    prepare();
    DAC->SR |= DAC_SR_DMAUDR1;
    state.dac.faulted = true;
    hold_underrun = true;
    EXPECT_NE(nx_dac_port_stop(&control, NX_DEADLINE_NEVER), NX_SUCCESS);
    EXPECT_TRUE(state.dac.faulted);
    hold_underrun = false;
    EXPECT_EQ(nx_dac_port_stop(&control, NX_DEADLINE_NEVER), NX_SUCCESS);
}
TEST_F(DacDma, AForeignStreamVisibleAfterClockEnableIsNeverAdopted) {
    foreign_stream_on_clock = true;
    EXPECT_EQ(nx_stm32_dac_dma_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_FALSE(state.route_owned);
    EXPECT_EQ(state.vector_leases, 0U);
    EXPECT_NE(DMA1_Stream5->CR & DMA_SxCR_EN, 0U);
    foreign_stream_on_clock = false;
    DMA1_Stream5->CR = 0U;
}
TEST_F(DacDma, ANewTimerUpdateDuringTailSetupIsNotAnIoFailure) {
    prepare();
    start();
    pulse();
    update_while_clearing = true;
    pulse();
    update_while_clearing = false;
    pulse();
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
}
TEST_F(DacDma, StartupCannotRunPastItsAbsoluteDeadline) {
    prepare();
    expire_on_dma_enable = true;
    start(2U, 10000U);
    EXPECT_EQ(TIM6->CR1 & TIM_CR1_CEN, 0U);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_TIMEOUT);
}
TEST_F(DacDma, QuarantineDoesNotRequestAContinuousBusyServiceLoop) {
    prepare();
    start();
    hold_stream = true;
    ASSERT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    clock_us += 1000U;
    nx_dac_tx_port_service(&tx);
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_dac_tx_port_next_service(&tx), NX_DEADLINE_NEVER);
}

TEST_F(DacDma, ExhaustedFiniteDrainBudgetNeverBecomesAnInfiniteDeadline) {
    prepare();
    start();
    hold_stream = true;
    clock_us = NX_DEADLINE_NEVER - 150U;
    ASSERT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    nx_dac_tx_port_service(&tx);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_LT(state.drain_deadline, NX_DEADLINE_NEVER);
    EXPECT_EQ(state.active, &request);
}

TEST_F(DacDma, CancelledPendingRequestCannotConsumeTheNextDescriptor) {
    prepare();
    start();
    delay_dma_ack = true;
    pulse(false);
    ASSERT_TRUE(request_pending[0]);
    ASSERT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_CANCELLED);
    delay_dma_ack = false;
    data += 32U;
    const uint16_t codes[] = {0x789U, 0xabcU};
    std::memcpy(data, codes, sizeof(codes));
    start();
    EXPECT_EQ(DMA1_Stream5->NDTR, 2U);
    EXPECT_EQ(DAC->DHR12R1, config.dac.idle_codes[0]);
    pulse();
    EXPECT_EQ(DMA1_Stream5->NDTR, 1U);
    EXPECT_EQ(DAC->DOR1, config.dac.idle_codes[0]);
    EXPECT_EQ(DAC->DHR12R1, 0x789U);
}

TEST_F(DacDma, LateFiniteTailPendingCannotShiftTheNextPlayback) {
    prepare();
    start();
    pulse(false);
    pulse(false);
    pulse(false);
    ASSERT_TRUE(request_pending[0]);
    nx_dac_tx_port_service(&tx);
    pulse();
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
    start();
    EXPECT_EQ(DMA1_Stream5->NDTR, 2U);
    EXPECT_EQ(DAC->DHR12R1, config.dac.idle_codes[0]);
}

TEST_F(DacDma, FailedTemporaryGateRestorationRetainsTheInheritedClockReceipt) {
    RCC->APB1ENR = RCC_APB1ENR_DACEN;
    prepare();
    request_pending[0] = true;
    refuse_dac_clock_enable = true;
    start();
    clock_us += 1000U;
    nx_dac_tx_port_service(&tx);
    EXPECT_EQ(RCC->APB1ENR & RCC_APB1ENR_DACEN, 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(nx_dac_port_stop(&control, NX_DEADLINE_NEVER), NX_ERROR_BUSY);
    EXPECT_EQ(state.active, &request);
    nx_dac_state_t output{};
    output.trigger_divisor = 123U;
    EXPECT_EQ(nx_dac_port_state(&control, &output), NX_ERROR_STATE);
    EXPECT_EQ(output.trigger_divisor, 123U);
    EXPECT_NE(nx_stm32_dac_dma_close(&state), NX_SUCCESS);
    EXPECT_NE(state.dac.config, nullptr);
    EXPECT_EQ(state.active, &request);
    refuse_dac_clock_enable = false;
    EXPECT_EQ(nx_stm32_dac_dma_close(&state), NX_SUCCESS);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_DACEN, 0U);
    result(NX_ERROR_IO);
}

TEST_F(DacDma, MissingHardwareUnderrunCannotBeReplacedByAClockCycle) {
    prepare();
    suppress_hardware_trigger = true;
    const auto began = clock_us;
    start();
    EXPECT_LT(clock_us - began, 100U);
    EXPECT_FALSE(dac_clock_withdrawn);
    EXPECT_EQ(DMA1_Stream5->NDTR, 0U);
    EXPECT_EQ(DMA1_Stream5->CR & DMA_SxCR_EN, 0U);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_IO);
}

TEST_F(DacDma, RefusedGateWithdrawalFailsWithoutLosingTheClockReceipt) {
    RCC->APB1ENR = RCC_APB1ENR_DACEN;
    prepare();
    refuse_dac_clock_disable = true;
    start();
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_DACEN, 0U);
    EXPECT_EQ(DMA1_Stream5->CR & DMA_SxCR_EN, 0U);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_IO);
    EXPECT_EQ(nx_stm32_dac_dma_close(&state), NX_SUCCESS);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_DACEN, 0U);
}

TEST_F(DacDma, ARecoveryTriggerCannotRunBeyondTheAcceptedDeadline) {
    for (bool pending : {false, true}) {
        SCOPED_TRACE(pending);
        clock_us = 100U;
        prepare();
        request_pending[0] = pending;
        expire_on_recovery_trigger = true;
        start(2U, 10000U);
        EXPECT_EQ(DMA1_Stream5->M0AR, 0U);
        EXPECT_EQ(DMA1_Stream5->CR & DMA_SxCR_EN, 0U);
        nx_dac_tx_port_service(&tx);
        result(NX_ERROR_TIMEOUT);
        EXPECT_FALSE(state.dac.faulted);
        expire_on_recovery_trigger = false;
        ASSERT_EQ(nx_stm32_dac_dma_close(&state), NX_SUCCESS);
    }
}

TEST_F(DacDma, RefusingToClearInternalUnderrunKeepsTheFailedBorrow) {
    prepare();
    request_pending[0] = true;
    expire_on_recovery_trigger = hold_underrun = true;
    start(2U, 10000U);
    clock_us += 1000U;
    nx_dac_tx_port_service(&tx);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_TRUE(state.dac.faulted);
    hold_underrun = false;
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_IO);
}

class DacDmaFormat : public DacDma,
                     public testing::WithParamInterface<unsigned> {};
TEST_P(DacDmaFormat, InheritedPendingRequiresAFullRestartBeforeFirstArm) {
    const unsigned selection = GetParam();
    config.dac.channels = static_cast<uint8_t>(selection / 3U + 1U);
    config.dac.format = static_cast<nx_dac_format_t>(selection % 3U);
    config.dac.trigger = selection % 2U == 0U ? NX_STM32_DAC_TRIGGER_TIM6
                                              : NX_STM32_DAC_TRIGGER_TIM7;
    width = (config.dac.format == NX_DAC_FORMAT_U8 ? 1U : 2U) *
            (config.dac.channels == 3U ? 2U : 1U);
    const uint32_t encoded =
        config.dac.format == NX_DAC_FORMAT_U8
            ? (config.dac.channels == 3U ? 0x4512U : 0x12U)
        : config.dac.format == NX_DAC_FORMAT_U12_LEFT
            ? (config.dac.channels == 3U ? 0x45601230U : 0x1230U)
            : (config.dac.channels == 3U ? 0x04560123U : 0x123U);
    std::memcpy(data, &encoded, width);
    std::memcpy(data + width, &encoded, width);
    const unsigned index = config.dac.channels == 2U ? 1U : 0U;
    request_pending[index] = true;
    RCC->APB1ENR = RCC_APB1ENR_DACEN;
    nx_dac_model_write(&DAC->CR, 0U);
    nx_dac_model_write(&RCC->APB1ENR, 0U);
    nx_dac_model_write(&DAC->SR, underrun_mask(config.dac.channels));
    nx_dac_model_write(&RCC->APB1ENR, RCC_APB1ENR_DACEN);
    ASSERT_TRUE(request_pending[index]);
    prepare();
    start();
    EXPECT_EQ(stream_for(config.dac.channels)->NDTR, 2U);
    pulse();
    EXPECT_EQ(stream_for(config.dac.channels)->NDTR, 1U);
}
TEST_P(DacDmaFormat, ExactFrameWidthAndRequestRouteMatchSelectedChannels) {
    const unsigned selection = GetParam();
    config.dac.channels = static_cast<uint8_t>(selection / 3U + 1U);
    config.dac.format = static_cast<nx_dac_format_t>(selection % 3U);
    width = (config.dac.format == NX_DAC_FORMAT_U8 ? 1U : 2U) *
            (config.dac.channels == 3U ? 2U : 1U);
    const bool dual = config.dac.channels == 3U;
    const uint32_t encoded = config.dac.format == NX_DAC_FORMAT_U8
                                 ? (dual ? 0x4512U : 0x12U)
                             : config.dac.format == NX_DAC_FORMAT_U12_LEFT
                                 ? (dual ? 0x45601230U : 0x1230U)
                                 : (dual ? 0x04560123U : 0x123U);
    std::memcpy(data, &encoded, width);
    std::memcpy(data + width, &encoded, width);
    prepare();
    start();
    auto* stream = stream_for(config.dac.channels);
    EXPECT_EQ(stream->NDTR, 2U);
    EXPECT_NE(NVIC_GetEnableIRQ(stream_irq(config.dac.channels)), 0U);
    EXPECT_EQ(stream->M0AR, reinterpret_cast<uintptr_t>(data));
    EXPECT_EQ((stream->CR & DMA_SxCR_CHSEL) >> DMA_SxCR_CHSEL_Pos, 7U);
    EXPECT_EQ(DAC->CR & (DAC_CR_DMAEN1 | DAC_CR_DMAEN2),
              request_mask(config.dac.channels));
    const uint32_t expected_width = width == 1U ? 0U : width == 2U ? 1U : 2U;
    EXPECT_EQ((stream->CR & DMA_SxCR_MSIZE) >> DMA_SxCR_MSIZE_Pos,
              expected_width);
    EXPECT_EQ((stream->CR & DMA_SxCR_PSIZE) >> DMA_SxCR_PSIZE_Pos,
              expected_width);
    const volatile uint32_t* expected =
        dual ? (config.dac.format == NX_DAC_FORMAT_U8         ? &DAC->DHR8RD
                : config.dac.format == NX_DAC_FORMAT_U12_LEFT ? &DAC->DHR12LD
                                                              : &DAC->DHR12RD)
        : config.dac.channels == 1U
            ? (config.dac.format == NX_DAC_FORMAT_U8         ? &DAC->DHR8R1
               : config.dac.format == NX_DAC_FORMAT_U12_LEFT ? &DAC->DHR12L1
                                                             : &DAC->DHR12R1)
            : (config.dac.format == NX_DAC_FORMAT_U8         ? &DAC->DHR8R2
               : config.dac.format == NX_DAC_FORMAT_U12_LEFT ? &DAC->DHR12L2
                                                             : &DAC->DHR12R2);
    EXPECT_EQ(stream->PAR, reinterpret_cast<uintptr_t>(expected));
    pulse();
    pulse();
    pulse();
    nx_dac_tx_port_service(&tx);
    result(NX_SUCCESS);
    const uint32_t first =
        config.dac.format == NX_DAC_FORMAT_U8 ? 0x120U : 0x123U;
    const uint32_t second =
        dual ? (config.dac.format == NX_DAC_FORMAT_U8 ? 0x450U : 0x456U)
             : first;
    EXPECT_EQ(DAC->DOR1, (config.dac.channels & 1U) != 0U ? first : 0U);
    EXPECT_EQ(DAC->DOR2, (config.dac.channels & 2U) != 0U ? second : 0U);
}
INSTANTIATE_TEST_SUITE_P(SelectedLayout, DacDmaFormat, testing::Range(0U, 9U));

class DacExternalDma : public DacDma {
  protected:
    nx_stm32_exti_hardware_config_t event_config{GPIOB, NX_EXTI_RISING, 1U, 9U,
                                                 0U};
    nx_stm32_exti_hardware_state_t event{};
    void SelectSource(bool circular = true) {
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
        RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
        ASSERT_EQ(nx_stm32_exti_hardware_prepare(&event, &event_config),
                  NX_SUCCESS);
        config.dac.trigger = NX_STM32_DAC_TRIGGER_EXTI9;
        config.dac.external = &event;
        config.circular = circular;
        config.recovery_trigger = NX_STM32_DAC_TRIGGER_TIM6;
    }
    void StartExternal() {
        ASSERT_EQ(nx_dac_tx_prepare(&request, data, 2U * width, 0U, 10000U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_dac_tx_port_submit(&tx, &request), NX_SUCCESS);
        ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    }
    void Edge() {
        clock_us += 100U;
        hardware_trigger(nullptr);
        if ((DAC->SR & underrun_mask(config.dac.channels)) != 0U)
            irq();
    }
    void Close() {
        ASSERT_EQ(nx_stm32_dac_dma_close(&state), NX_SUCCESS);
        ASSERT_EQ(nx_stm32_exti_hardware_close(&event), NX_SUCCESS);
    }
};

TEST_F(DacExternalDma,
       CircularPlaybackRequiresElectricalEdgesNotRecoveryTimer) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_NO_FATAL_FAILURE(prepare());
    ASSERT_NO_FATAL_FAILURE(StartExternal());
    EXPECT_EQ(next_frame[0], 0U);
    EXPECT_EQ(TIM6->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(TIM7->CR1 & TIM_CR1_CEN, 0U);
    EXPECT_EQ(DAC->CR & DAC_CR_TSEL1, DAC_CR_TSEL1_2 | DAC_CR_TSEL1_1);
    Edge();
    EXPECT_EQ(DAC->DOR1, 0U);
    Edge();
    EXPECT_EQ(DAC->DOR1, 0x123U);
    Edge();
    EXPECT_EQ(DAC->DOR1, 0x456U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_FALSE(changed_tsel_while_enabled);
    nx_dac_state_t observed{};
    ASSERT_EQ(nx_dac_port_state(&control, &observed), NX_SUCCESS);
    EXPECT_EQ(observed.timer_clock_hz | observed.trigger_divisor, 0U);
    ASSERT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_CANCELLED);
    ASSERT_NO_FATAL_FAILURE(Close());
}

TEST_F(DacExternalDma, FiniteExternalPlaybackRejectsWithoutInventingATail) {
    ASSERT_NO_FATAL_FAILURE(SelectSource(false));
    const size_t before = stores.size();
    EXPECT_EQ(nx_stm32_dac_dma_prepare(&state, &config), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(stores.size(), before);
    EXPECT_EQ(state.dac.config, nullptr);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    ASSERT_NO_FATAL_FAILURE(Close());
}

TEST_F(DacExternalDma, RecoveryTimerMustBeExplicitAndExclusivelyIdle) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    config.recovery_trigger = NX_STM32_DAC_TRIGGER_NONE;
    const size_t before = stores.size();
    EXPECT_EQ(nx_stm32_dac_dma_prepare(&state, &config), NX_ERROR_INVALID);
    config.recovery_trigger = NX_STM32_DAC_TRIGGER_TIM6;
    TIM6->CR1 = TIM_CR1_CEN;
    EXPECT_EQ(nx_stm32_dac_dma_prepare(&state, &config), NX_ERROR_BUSY);
    EXPECT_EQ(stores.size(), before);
    EXPECT_EQ(state.dac.config, nullptr);
    TIM6->CR1 = 0U;
    ASSERT_NO_FATAL_FAILURE(Close());
}

TEST_F(DacExternalDma, ChannelTwoCanLeaseTimSevenOnlyForRecovery) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    config.dac.channels = 2U;
    config.recovery_trigger = NX_STM32_DAC_TRIGGER_TIM7;
    ASSERT_NO_FATAL_FAILURE(prepare());
    ASSERT_NO_FATAL_FAILURE(StartExternal());
    EXPECT_NE(DMA1_Stream6->CR & DMA_SxCR_EN, 0U);
    EXPECT_EQ(DMA1_Stream5->CR & DMA_SxCR_EN, 0U);
    EXPECT_NE(RCC->APB1ENR & RCC_APB1ENR_TIM7EN, 0U);
    EXPECT_EQ(RCC->APB1ENR & RCC_APB1ENR_TIM6EN, 0U);
    EXPECT_EQ((DAC->CR >> 16U) & DAC_CR_TSEL1, DAC_CR_TSEL1_2 | DAC_CR_TSEL1_1);
    EXPECT_EQ(TIM7->CR1 & TIM_CR1_CEN, 0U);
    Edge();
    Edge();
    EXPECT_EQ(DAC->DOR2, 0x123U);
    EXPECT_FALSE(changed_tsel_while_enabled);
    ASSERT_NO_FATAL_FAILURE(Close());
}

TEST_F(DacExternalDma, InternalTimerStillRejectsUnknownRateBeforeAdmission) {
    ASSERT_NO_FATAL_FAILURE(prepare());
    ASSERT_EQ(nx_dac_tx_prepare(&request, data, 4U, 0U, 10000U), NX_SUCCESS);
    const size_t before = stores.size();
    EXPECT_EQ(nx_dac_tx_port_submit(&tx, &request), NX_ERROR_INVALID);
    EXPECT_EQ(stores.size(), before);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    ASSERT_NO_FATAL_FAILURE(Close());
}

TEST_F(DacExternalDma, CancelledExternalPendingCannotPrefetchTheNextPayload) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_NO_FATAL_FAILURE(prepare());
    ASSERT_NO_FATAL_FAILURE(StartExternal());
    delay_dma_ack = true;
    Edge();
    ASSERT_TRUE(request_pending[0]);
    ASSERT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_CANCELLED);
    delay_dma_ack = false;
    const uint16_t next[] = {0x789U, 0xabcU};
    std::memcpy(data, next, sizeof(next));
    ASSERT_NO_FATAL_FAILURE(StartExternal());
    EXPECT_EQ(next_frame[0], 0U);
    EXPECT_FALSE(request_pending[0]);
    EXPECT_FALSE(underrun_blocked[0]);
    Edge();
    EXPECT_EQ(DAC->DOR1, 0U);
    Edge();
    EXPECT_EQ(DAC->DOR1, 0x789U);
    EXPECT_FALSE(changed_tsel_while_enabled);
    ASSERT_NO_FATAL_FAILURE(Close());
}

TEST_F(DacExternalDma, LostEventOwnerRetainsAnUndrainedCircularBorrow) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_NO_FATAL_FAILURE(prepare());
    ASSERT_NO_FATAL_FAILURE(StartExternal());
    ASSERT_EQ(nx_stm32_exti_hardware_close(&event), NX_SUCCESS);
    hold_stream = true;
    nx_dac_tx_port_service(&tx);
    clock_us += 201U;
    nx_dac_tx_port_service(&tx);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_NE(DMA1_Stream5->M0AR, 0U);
    hold_stream = false;
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_IO);
    ASSERT_NO_FATAL_FAILURE(Close());
}

TEST_F(DacExternalDma, RefusedExternalTriggerWithdrawalKeepsTheMemoryLoan) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_NO_FATAL_FAILURE(prepare());
    ASSERT_NO_FATAL_FAILURE(StartExternal());
    hold_trigger = true;
    ASSERT_EQ(nx_dac_tx_port_cancel(&tx, &request), NX_SUCCESS);
    nx_dac_tx_port_service(&tx);
    clock_us += 201U;
    nx_dac_tx_port_service(&tx);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_NE(DMA1_Stream5->M0AR, 0U);
    hold_trigger = false;
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_CANCELLED);
    ASSERT_NO_FATAL_FAILURE(Close());
}

TEST_F(DacExternalDma, ExternalOwnerStorageCannotBecomeImmutableDmaPayload) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_EQ(nx_stm32_exti_hardware_close(&event), NX_SUCCESS);
    auto* owner = new (reinterpret_cast<void*>(0x20003000U))
        nx_stm32_exti_hardware_state_t{};
    ASSERT_EQ(nx_stm32_exti_hardware_prepare(owner, &event_config), NX_SUCCESS);
    config.dac.external = owner;
    ASSERT_NO_FATAL_FAILURE(prepare());
    ASSERT_EQ(nx_dac_tx_prepare(&request, owner, sizeof(*owner), 0U, 10000U),
              NX_SUCCESS);
    const size_t before = stores.size();
    EXPECT_EQ(nx_dac_tx_port_submit(&tx, &request), NX_ERROR_INVALID);
    EXPECT_EQ(stores.size(), before);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    ASSERT_EQ(nx_stm32_dac_dma_close(&state), NX_SUCCESS);
    ASSERT_EQ(nx_stm32_exti_hardware_close(owner), NX_SUCCESS);
}

TEST_F(DacExternalDma, InvalidTimeAfterDmaEnableCannotOpenExternalDetection) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_NO_FATAL_FAILURE(prepare());
    ASSERT_EQ(nx_dac_tx_prepare(&request, data, 4U, 0U, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    lose_time_on_dma_enable = true;
    ASSERT_EQ(nx_dac_tx_port_submit(&tx, &request), NX_SUCCESS);
    EXPECT_TRUE(clock_lost);
    EXPECT_EQ(DAC->CR & DAC_CR_TEN1, 0U);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_IO);
    EXPECT_EQ(DMA1_Stream5->CR & DMA_SxCR_EN, 0U);
    clock_lost = lose_time_on_dma_enable = false;
    ASSERT_NO_FATAL_FAILURE(Close());
}

TEST_F(DacExternalDma,
       InvalidTimeDuringRecoveryWaitCannotAuthorizeTheNewStream) {
    ASSERT_NO_FATAL_FAILURE(SelectSource());
    ASSERT_NO_FATAL_FAILURE(prepare());
    ASSERT_EQ(nx_dac_tx_prepare(&request, data, 4U, 0U, NX_DEADLINE_NEVER),
              NX_SUCCESS);
    lose_time_in_recovery_wait = true;
    ASSERT_EQ(nx_dac_tx_port_submit(&tx, &request), NX_SUCCESS);
    EXPECT_TRUE(clock_lost);
    EXPECT_EQ(DAC->CR & DAC_CR_TEN1, 0U);
    EXPECT_EQ(DMA1_Stream5->CR & DMA_SxCR_EN, 0U);
    nx_dac_tx_port_service(&tx);
    result(NX_ERROR_IO);
    clock_lost = lose_time_in_recovery_wait = false;
    ASSERT_NO_FATAL_FAILURE(Close());
}
