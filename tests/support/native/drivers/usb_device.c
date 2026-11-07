/**
 * \file            usb_device.c
 * \brief           Opt-in Native FS PIO register and acknowledgement model
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "usb_device.h"
#include <string.h>

/** \brief Model clock acquisition still requires actual injected ready facts.
 */
static nx_result_t clock_validate(const void* context) {
    const nx_native_usb_hardware_t* hw = context;
    return hw->clock_ready ? NX_SUCCESS : NX_ERROR_STATE;
}
static nx_result_t clock_retain(void* context) {
    nx_native_usb_hardware_t* hw = context;
    if (!hw->clock_ready) {
        return NX_ERROR_STATE;
    }
    if (hw->consumers == UINT32_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    ++hw->consumers;
    return NX_SUCCESS;
}
static nx_result_t clock_release(void* context) {
    nx_native_usb_hardware_t* hw = context;
    if (hw->consumers == 0U) {
        return NX_ERROR_STATE;
    }
    --hw->consumers;
    return NX_SUCCESS;
}
static const nx_usb_clock_ops_t clock_ops = {.retain = clock_retain,
                                             .release = clock_release,
                                             .validate = clock_validate};
/** \brief Recompute level endpoint summary without clearing unrelated W1C bits.
 */
static void summaries(nx_native_usb_hardware_t* hw) {
    uint32_t summary = 0U;
    for (unsigned ep = 0U; ep < 4U; ++ep) {
        if (hw->registers[(0x900U + ep * 32U + 8U) / 4U] != 0U) {
            summary |= 1U << ep;
        }
        if (hw->registers[(0xB00U + ep * 32U + 8U) / 4U] != 0U) {
            summary |= 1U << (ep + 16U);
        }
    }
    hw->registers[0x818U / 4U] = summary;
    hw->registers[0x14U / 4U] &= ~((1U << 18U) | (1U << 19U) | 16U);
    if ((summary & 15U) != 0U) {
        hw->registers[0x14U / 4U] |= 1U << 18U;
    }
    if ((summary & 0xF0000U) != 0U) {
        hw->registers[0x14U / 4U] |= 1U << 19U;
    }
    if (hw->rx_count != 0U) {
        hw->registers[0x14U / 4U] |= 16U;
    }
}
/** \brief Cold model does not reset active software loans. */
nx_result_t nx_native_usb_initialize(nx_native_usb_device_t* model) {
    if (model == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_usb_controller_t* c = &model->controller;
    if (!c->snapshot.started) {
        model->hardware.registers[0x10U / 4U] = 1U << 31U;
    }
    c->registers = (uintptr_t)&model->hardware;
    c->clock = (nx_usb_clock_domain_t){.ops = &clock_ops,
                                       .context = &model->hardware,
                                       .context_size = sizeof(model->hardware)};
    return nx_usb_controller_initialize(c);
}
/** \brief Pop external RX status and then expose its FIFO words. */
uint32_t nx_native_usb_read(uintptr_t base, uint32_t offset) {
    nx_native_usb_hardware_t* hw = (nx_native_usb_hardware_t*)base;
    ++hw->mmio_reads;
    if (offset == 0x10U) {
        ++hw->reset_reads;
    }
    if (offset == 0x10U && (hw->registers[offset / 4U] & 0x31U) != 0U) {
        nx_native_usb_write(base, offset, hw->registers[offset / 4U]);
    }
    if (offset == 0x20U) {
        if (hw->rx_count == 0U) {
            return 0U;
        }
        hw->current = hw->rx[hw->rx_head];
        hw->rx_head = (hw->rx_head + 1U) % 16U;
        --hw->rx_count;
        hw->rx_word = 0U;
        summaries(hw);
        return hw->current.status;
    }
    if (offset == 0x1000U) {
        return hw->rx_word < 16U ? hw->current.words[hw->rx_word++] : 0U;
    }
    if (offset >= 0x908U && offset < 0x980U && offset % 32U == 8U) {
        unsigned ep = (offset - 0x908U) / 32U;
        return (hw->registers[offset / 4U] & ~128U) |
               (hw->tx_count[ep] == 0U ? 128U : 0U);
    }
    return offset < 0x1000U ? hw->registers[offset / 4U] : 0U;
}
/** \brief Model only hardware-side W1C, self-clear and disable
 * acknowledgements. */
void nx_native_usb_write(uintptr_t base, uint32_t offset, uint32_t value) {
    nx_native_usb_hardware_t* hw = (nx_native_usb_hardware_t*)base;
    ++hw->mmio_writes;
    if (offset >= 0x1000U) {
        unsigned ep = offset / 0x1000U - 1U;
        if (ep < 4U && hw->tx_count[ep] < 16U) {
            hw->tx[ep][hw->tx_count[ep]++] = value;
            --hw->registers[(0x900U + ep * 32U + 24U) / 4U];
        }
        return;
    }
    if (offset == 0x14U || (((offset >= 0x900U && offset < 0x980U) ||
                             (offset >= 0xB00U && offset < 0xB80U)) &&
                            (offset % 32U) == 8U)) {
        if (!hw->hold_clear) {
            hw->registers[offset / 4U] &= ~value;
        }
        summaries(hw);
        return;
    }
    if (offset == 0x81CU) {
        /* Fault only the selected stored mask bits, never provider state. */
        uint32_t held = hw->hold_ep_mask_bits;
        hw->registers[offset / 4U] =
            (value & ~held) | (hw->registers[offset / 4U] & held);
        return;
    }
    if (offset == 0x10U) {
        uint32_t command = value;
        if (!hw->hold_reset) {
            command &= ~1U;
        }
        if (!hw->hold_flush) {
            command &= ~0x30U;
            if ((value & 16U) != 0U) {
                hw->rx_count = hw->rx_head = 0U;
            }
            if ((value & 32U) != 0U) {
                unsigned fifo = (value >> 6U) & 31U;
                for (unsigned i = 0U; i < 4U; ++i) {
                    if (fifo == i || fifo == 16U) {
                        hw->tx_count[i] = 0U;
                        uint32_t size =
                            hw->registers[(i == 0U ? 0x28U : 0x100U + i * 4U) /
                                          4U];
                        hw->registers[(0x900U + i * 32U + 24U) / 4U] =
                            size >> 16U;
                    }
                }
            }
        }
        hw->registers[offset / 4U] = command | (1U << 31U);
        summaries(hw);
        return;
    }
    if (offset == 0x800U && hw->hold_address_clear && (value & 0x7F0U) == 0U) {
        value |= hw->registers[offset / 4U] & 0x7F0U;
    }
    if (offset == 0x804U) {
        uint32_t result = value & ~((1U << 9U) | (1U << 10U));
        if (hw->hold_disconnect) {
            result = (result & ~2U) | (hw->registers[offset / 4U] & 2U);
        }
        if ((value & (1U << 9U)) != 0U && !hw->hold_gonak) {
            result |= 8U;
        }
        if ((value & (1U << 10U)) != 0U) {
            result &= ~8U;
        }
        hw->registers[offset / 4U] = result;
        return;
    }
    bool in = offset >= 0x900U && offset < 0x980U;
    bool out = offset >= 0xB00U && offset < 0xB80U;
    if ((in || out) && offset % 32U == 0U) {
        if ((offset == 0x900U || offset == 0xB00U) &&
            (value & (3U << 28U)) != 0U) {
            hw->reserved_ep0_pid_command = true;
        }
        uint32_t previous = hw->registers[offset / 4U];
        unsigned number = (offset - (in ? 0x900U : 0xB00U)) / 32U;
        unsigned direction = number + (in ? 0U : 4U);
        if ((hw->hold_stall_clear_mask & (1U << direction)) != 0U) {
            value |= previous & (1U << 21U);
        }
        if (in && (value & (1U << 30U)) != 0U &&
            (previous & (1U << 17U)) == 0U) {
            hw->early_in_disable = true;
        }
        if (out && offset == 0xB00U && (value & (1U << 30U)) != 0U) {
            hw->out_zero_disable_requested = true;
        }
        uint32_t result =
            value & ~((1U << 27U) | (1U << 26U) | (1U << 29U) | (1U << 28U));
        bool zero = offset == 0x900U || offset == 0xB00U;
        if (!zero && (value & (1U << 29U)) != 0U) {
            result |= 1U << 16U;
        }
        if (zero || ((value & (1U << 28U)) != 0U && !hw->hold_bulk_pid)) {
            result &= ~(1U << 16U);
        }
        if ((value & (1U << 27U)) != 0U && !(in && hw->hold_in_nak)) {
            result |= 1U << 17U;
        }
        if ((value & (1U << 26U)) != 0U) {
            result &= ~(1U << 17U);
        }
        if (in && (value & (1U << 27U)) != 0U && !hw->hold_in_nak) {
            hw->registers[(offset + 8U) / 4U] |= 64U;
        }
        if ((value & (1U << 26U)) != 0U && in) {
            hw->registers[(offset + 8U) / 4U] &= ~64U;
        }
        if ((value & (1U << 30U)) != 0U && !hw->hold_disable &&
            !(out && offset == 0xB00U) &&
            !(in && (previous & (1U << 17U)) == 0U)) {
            result &= ~((1U << 31U) | (1U << 30U));
            hw->registers[(offset + 8U) / 4U] |= 2U;
        }
        if (out && offset == 0xB00U) {
            result &= ~(1U << 30U);
        }
        hw->registers[offset / 4U] = result;
        summaries(hw);
        return;
    }
    hw->registers[offset / 4U] = value;
    if (offset == 8U && value == 0U && hw->on_source_disable != NULL) {
        hw->on_source_disable(hw->disable_context);
    }
    if (offset == 8U && value == 1U && hw->on_interrupt_enable != NULL) {
        hw->on_interrupt_enable(hw->interrupt_context);
    }
    if (offset == 0x28U || (offset >= 0x104U && offset <= 0x10CU)) {
        unsigned ep = offset == 0x28U ? 0U : (offset - 0x100U) / 4U;
        hw->registers[(0x900U + ep * 32U + 24U) / 4U] = value >> 16U;
    }
}
nx_result_t nx_native_usb_receive(nx_native_usb_device_t* model, uint8_t ep,
                                  const uint8_t* bytes, size_t length,
                                  bool setup) {
    if (model == NULL || ep > 3U || length > 64U ||
        (length != 0U && bytes == NULL)) {
        return NX_ERROR_INVALID;
    }
    nx_native_usb_hardware_t* hw = &model->hardware;
    if (hw->rx_count == 16U) {
        return NX_ERROR_OVERFLOW;
    }
    size_t tail = (hw->rx_head + hw->rx_count) % 16U;
    nx_native_usb_packet_t* packet = &hw->rx[tail];
    *packet = (nx_native_usb_packet_t){.status = ep | ((uint32_t)length << 4U) |
                                                 ((setup ? 6U : 2U) << 17U)};
    for (size_t i = 0U; i < length; ++i) {
        packet->words[i / 4U] |= (uint32_t)bytes[i] << ((i % 4U) * 8U);
    }
    ++hw->rx_count;
    summaries(hw);
    return NX_SUCCESS;
}
nx_result_t nx_native_usb_complete(nx_native_usb_device_t* model, uint8_t ep,
                                   bool in) {
    if (model == NULL || ep > 3U) {
        return NX_ERROR_INVALID;
    }
    uint32_t offset = (in ? 0x900U : 0xB00U) + (uint32_t)ep * 32U;
    model->hardware.registers[offset / 4U] &= ~(1U << 31U);
    model->hardware.registers[(offset + 8U) / 4U] |= 1U;
    if (in) {
        if (ep != 0U) {
            model->hardware.registers[offset / 4U] ^= 1U << 16U;
        }
        uint32_t size =
            model->hardware
                .registers[(ep == 0U ? 0x28U : 0x100U + (uint32_t)ep * 4U) /
                           4U];
        model->hardware.registers[(offset + 24U) / 4U] = size >> 16U;
        model->hardware.tx_count[ep] = 0U;
    }
    summaries(&model->hardware);
    return NX_SUCCESS;
}
void nx_native_usb_reset(nx_native_usb_device_t* model) {
    if (model != NULL) {
        /* ST FS bus reset clears USBAEP on every nonzero direction. This
         * physical-port reference does not infer the GD reset side effect. */
        for (uint32_t number = 1U; number < 4U; ++number) {
            model->hardware.registers[(0x900U + number * 32U) / 4U] &=
                ~(1U << 15U);
            model->hardware.registers[(0xB00U + number * 32U) / 4U] &=
                ~(1U << 15U);
        }
        model->hardware.registers[0x14U / 4U] |= 1U << 12U;
    }
}
void nx_native_usb_irq_enable(uintptr_t base, int irq, uint8_t priority,
                              uint8_t bits) {
    (void)irq;
    (void)bits;
    nx_native_usb_hardware_t* hw = (nx_native_usb_hardware_t*)base;
    hw->irq_enabled = true;
    hw->priority = priority;
}
void nx_native_usb_irq_disable(uintptr_t base, int irq) {
    (void)irq;
    nx_native_usb_hardware_t* hw = (nx_native_usb_hardware_t*)base;
    if (!hw->hold_irq_disable) {
        hw->irq_enabled = false;
    }
}
bool nx_native_usb_irq_enabled(uintptr_t base, int irq) {
    (void)irq;
    return ((nx_native_usb_hardware_t*)base)->irq_enabled;
}
uint8_t nx_native_usb_irq_priority(uintptr_t base, int irq, uint8_t bits) {
    (void)irq;
    (void)bits;
    return ((nx_native_usb_hardware_t*)base)->priority;
}
