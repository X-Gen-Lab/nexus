/**
 * \file            i2c.c
 * \brief           STM32F407 standard-mode I2C messages and bounded abort
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_i2c_cold.h"
#include "stm32f407_i2c_port.h"
#include "stm32f407_provider.h"
#include "stm32f4xx_ll_i2c.h"

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

nx_result_t nx_stm32_i2c_prepare(nx_stm32_i2c_state_t* port,
                                 const nx_stm32_i2c_config_t* config,
                                 nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL ||
        config->asynchronous)
        return NX_ERROR_INVALID;
    if (port->registers != NULL || port->active || port->initialized)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_stm32_i2c_cold_acquire(config, receipt);
    if (result != NX_SUCCESS)
        return result;
    port->registers = config->registers;
    port->line_gpio = config->registers == I2C3 ? GPIOA : GPIOB;
    port->line_mask = (1U << config->pins[0].pin) | (1U << config->pins[1].pin);
    port->peripheral_mhz = config->clock_hz / 1000000U;
    port->rate_hz = config->max_hz;
    result = nx_stm32_i2c_initialize(port);
    if (result == NX_SUCCESS) {
        receipt->effects |= NX_STM32_I2C_COLD_PREPARED;
        result = nx_stm32_i2c_cold_verify(config, receipt);
        if (result != NX_SUCCESS)
            (void)nx_stm32_i2c_close(port, config, receipt);
    } else if (nx_stm32_i2c_cold_release(config, receipt) == NX_SUCCESS) {
        *port = (nx_stm32_i2c_state_t){0};
    }
    return result;
}

nx_result_t nx_stm32_i2c_close(nx_stm32_i2c_state_t* port,
                               const nx_stm32_i2c_config_t* config,
                               nx_stm32_i2c_cold_receipt_t* receipt) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || receipt == NULL ||
        config->asynchronous)
        return NX_ERROR_INVALID;
    /* A rejected prepare did not acquire this existing owner's state. */
    if ((receipt->effects & NX_STM32_I2C_COLD_OWNED) == 0U)
        return port->registers == NULL ? NX_SUCCESS : NX_ERROR_BUSY;
    if ((receipt->effects & NX_STM32_I2C_COLD_PREPARED) != 0U) {
        if (port->registers != config->registers)
            return NX_ERROR_INVALID;
        if (port->active)
            return NX_ERROR_BUSY;
        NX_PLATFORM_REGISTER_WRITE(port->registers->CR1, 0U);
        nx_arch_dsb();
        if (port->registers->CR1 != 0U)
            return NX_ERROR_IO;
        port->initialized = false;
        receipt->effects &= (uint8_t)~NX_STM32_I2C_COLD_PREPARED;
    }
    nx_result_t result = nx_stm32_i2c_cold_release(config, receipt);
    if (result == NX_SUCCESS)
        *port = (nx_stm32_i2c_state_t){0};
    return result;
}

#ifndef NX_STM32_I2C_READ_DATA
#define NX_STM32_I2C_READ_DATA(port) LL_I2C_ReceiveData8((port)->registers)
#endif
#ifndef NX_STM32_I2C_WRITE_DATA
#define NX_STM32_I2C_WRITE_DATA(port, value)                                   \
    LL_I2C_TransmitData8((port)->registers, (uint8_t)(value))
#endif
#ifndef NX_STM32_I2C_ADDRESS_CLEARED
#define NX_STM32_I2C_ADDRESS_CLEARED(port) ((void)(port))
#endif

/** \brief Apply the selected whole-MHz APB standard/Fast timing. */
nx_result_t nx_stm32_i2c_initialize(nx_stm32_i2c_state_t* port) {
    if (port == NULL || port->registers == NULL || port->line_gpio == NULL ||
        port->line_mask == 0U || (port->line_mask & ~UINT32_C(0xFFFF)) != 0U) {
        return NX_ERROR_INVALID;
    }
    uint16_t ccr;
    uint8_t rise;
    if (NEXUS_APB1_HZ % 1000000U != 0U ||
        port->peripheral_mhz != NEXUS_APB1_HZ / 1000000U ||
        port->peripheral_mhz < 2U || port->peripheral_mhz > 50U ||
        !nx_i2c_v1_timing((uint8_t)port->peripheral_mhz, port->rate_hz, &ccr,
                          &rise)) {
        return NX_ERROR_UNSUPPORTED;
    }
    port->registers->CR1 = 0U;
    port->registers->CR2 = port->peripheral_mhz;
    port->registers->CCR = ccr;
    port->registers->TRISE = rise;
    port->registers->OAR1 = (1U << 14U);
    LL_I2C_Enable(port->registers);
    port->active = false;
    port->fault = false;
    port->initialized = true;
    return NX_SUCCESS;
}

/** \brief Observe real status flags and retain the original hardware error. */
static nx_result_t wait_flag(nx_stm32_i2c_state_t* port, uint32_t mask,
                             nx_time_us_t deadline) {
    for (;;) {
        NX_STM32_IO_POLL(2U, port);
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
        uint32_t status = port->registers->SR1;
        if ((status & I2C_SR1_AF) != 0U) {
            LL_I2C_ClearFlag_AF(port->registers);
            return NX_ERROR_NACK;
        }
        if ((status & I2C_SR1_ARLO) != 0U) {
            LL_I2C_ClearFlag_ARLO(port->registers);
            return NX_ERROR_ARBITRATION;
        }
        if ((status & (I2C_SR1_BERR | I2C_SR1_OVR)) != 0U) {
            return NX_ERROR_IO;
        }
        if ((status & mask) == mask) {
            return NX_SUCCESS;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
    }
}

/** \brief Clear ADDR with the RM0090 prescribed SR1/SR2 read sequence. */
static void clear_address(nx_stm32_i2c_state_t* port) {
    LL_I2C_ClearFlag_ADDR(port->registers);
    NX_STM32_I2C_ADDRESS_CLEARED(port);
}

/** \brief Request STOP or the next repeated START at the final receive window.
 */
static void end_receive(nx_stm32_i2c_state_t* port, bool last) {
    if (last) {
        LL_I2C_GenerateStopCondition(port->registers);
    } else {
        LL_I2C_GenerateStartCondition(port->registers);
    }
}

/** \brief Read with explicit one/two/final-three byte ACK timing. */
static nx_result_t receive(nx_stm32_i2c_state_t* port,
                           nx_i2c_message_t* message, bool last,
                           nx_time_us_t deadline, size_t* transferred) {
    I2C_TypeDef* regs = port->registers;
    size_t remaining = message->length;
    size_t index = 0U;
    nx_result_t result;
    if (remaining == 1U) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        LL_I2C_AcknowledgeNextData(regs, LL_I2C_NACK);
        clear_address(port);
        end_receive(port, last);
        nx_arch_irq_restore(mask);
        result = wait_flag(port, I2C_SR1_RXNE, deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        message->buffer.rx[0] = NX_STM32_I2C_READ_DATA(port);
        ++*transferred;
        return NX_SUCCESS;
    }
    if (remaining == 2U) {
        LL_I2C_EnableBitPOS(regs);
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        clear_address(port);
        LL_I2C_AcknowledgeNextData(regs, LL_I2C_NACK);
        nx_arch_irq_restore(mask);
        result = wait_flag(port, I2C_SR1_BTF, deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        mask = nx_arch_irq_save();
        end_receive(port, last);
        message->buffer.rx[0] = NX_STM32_I2C_READ_DATA(port);
        message->buffer.rx[1] = NX_STM32_I2C_READ_DATA(port);
        nx_arch_irq_restore(mask);
        *transferred += 2U;
        return NX_SUCCESS;
    }
    clear_address(port);
    while (remaining > 3U) {
        result = wait_flag(port, I2C_SR1_RXNE, deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        message->buffer.rx[index++] = NX_STM32_I2C_READ_DATA(port);
        --remaining;
        ++*transferred;
    }
    result = wait_flag(port, I2C_SR1_BTF, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    /* Keep final-three ACK withdrawal and N-2 consumption in one bounded
     * protected window. Hardware waits remain outside the interrupt mask. */
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    LL_I2C_AcknowledgeNextData(regs, LL_I2C_NACK);
    message->buffer.rx[index++] = NX_STM32_I2C_READ_DATA(port);
    nx_arch_irq_restore(mask);
    ++*transferred;
    result = wait_flag(port, I2C_SR1_BTF, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    mask = nx_arch_irq_save();
    end_receive(port, last);
    message->buffer.rx[index++] = NX_STM32_I2C_READ_DATA(port);
    message->buffer.rx[index] = NX_STM32_I2C_READ_DATA(port);
    nx_arch_irq_restore(mask);
    *transferred += 2U;
    return NX_SUCCESS;
}

/** \brief Bound STOP observation; reset local controller if the bus stays hung.
 */
static bool drain_stop(nx_stm32_i2c_state_t* port) {
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(nx_time_now_us(), 1000U, &deadline) !=
        NX_SUCCESS)
        deadline = 0U;
    while ((port->registers->CR1 & I2C_CR1_STOP) != 0U ||
           (port->registers->SR2 & I2C_SR2_MSL) != 0U) {
        NX_STM32_IO_POLL(2U, port);
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            port->registers->CR1 = I2C_CR1_SWRST;
            nx_arch_dsb();
            port->registers->CR1 = 0U;
            port->fault = true;
            return false;
        }
    }
    return true;
}

/** \brief Polling borrows cannot alias this exact controller's known bytes. */
static bool polling_storage(const nx_stm32_i2c_endpoint_state_t* endpoint,
                            const void* storage, size_t bytes) {
    const nx_stm32_i2c_state_t* port = endpoint->port;
    return nx_i2c_v1_storage_outside(storage, bytes, port, sizeof(*port)) &&
           nx_i2c_v1_storage_outside(storage, bytes, endpoint,
                                     sizeof(*endpoint)) &&
           nx_i2c_v1_storage_outside(storage, bytes, &nx_stm32_i2c_ops,
                                     sizeof(nx_stm32_i2c_ops)) &&
           nx_i2c_v1_storage_outside(storage, bytes, &nx_stm32_i2c_endpoint_ops,
                                     sizeof(nx_stm32_i2c_endpoint_ops)) &&
           nx_i2c_v1_storage_outside(storage, bytes, port->registers,
                                     sizeof(*port->registers)) &&
           nx_i2c_v1_storage_outside(storage, bytes, port->line_gpio,
                                     sizeof(*port->line_gpio)) &&
           (port->registers != I2C3 ||
            nx_i2c_v1_storage_outside(storage, bytes, GPIOC, sizeof(*GPIOC)));
}

/** \brief Reuse public message rules before the first progress/payload write.
 */
static nx_result_t
polling_request(const nx_stm32_i2c_endpoint_state_t* endpoint,
                nx_i2c_message_t* messages, size_t count, size_t* transferred) {
    size_t descriptors = count * sizeof(*messages);
    if ((uintptr_t)transferred % _Alignof(size_t) != 0U ||
        !polling_storage(endpoint, messages, descriptors) ||
        !polling_storage(endpoint, transferred, sizeof(*transferred)) ||
        !nx_i2c_v1_storage_outside(transferred, sizeof(*transferred), messages,
                                   descriptors))
        return NX_ERROR_INVALID;
    /* This local view is never admitted, initialized or retained. The existing
     * validator owns bounded message shape and RX/immutable-TX separation. */
    const nx_i2c_request_t view = {.messages = messages, .count = count};
    nx_result_t result =
        nx_i2c_request_validate_limits(&view, &nx_i2c_bounded_limits);
    if (result != NX_SUCCESS)
        return result;
    for (size_t index = 0U; index < count; ++index) {
        const nx_i2c_message_t* message = &messages[index];
        if (!polling_storage(endpoint, nx_i2c_message_data(message),
                             message->length) ||
            !nx_i2c_v1_storage_outside(nx_i2c_message_data(message),
                                       message->length, transferred,
                                       sizeof(*transferred)))
            return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}

/** \brief Execute a fixed-address, explicitly bounded repeated-START sequence.
 */
nx_result_t nx_stm32_i2c_endpoint_transaction(void* context,
                                              nx_i2c_message_t* messages,
                                              size_t count,
                                              nx_time_us_t deadline,
                                              size_t* transferred) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    const nx_stm32_i2c_endpoint_state_t* endpoint = context;
    if (endpoint == NULL || endpoint->port == NULL ||
        !nx_i2c_v1_address_valid(endpoint->address, endpoint->address_bits) ||
        messages == NULL || count == 0U || transferred == NULL) {
        return NX_ERROR_INVALID;
    }
    if (count > NX_I2C_BOUNDED_MAX_MESSAGES) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_stm32_i2c_state_t* port = endpoint->port;
    if (port->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result =
        polling_request(endpoint, messages, count, transferred);
    if (result != NX_SUCCESS)
        return result;
    *transferred = 0U;
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (port->active || (port->registers->SR2 & I2C_SR2_BUSY) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!port->initialized || port->fault ||
        (port->registers->CR1 & I2C_CR1_PE) == 0U) {
        return NX_ERROR_STATE;
    }
    if ((endpoint->address_bits == 10U || count > 1U) &&
        !nx_i2c_v1_restart_timing((uint16_t)port->registers->CCR,
                                  (uint8_t)port->peripheral_mhz)) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    result = NX_SUCCESS;
    bool start_requested = false;
    for (size_t message = 0U; message < count; ++message) {
        if (!start_requested) {
            LL_I2C_GenerateStartCondition(port->registers);
        }
        result = wait_flag(port, I2C_SR1_SB, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        /* SB proves the requested START completed. Prepare the next ACK/POS
         * only now: a CR1 RMW before SB could copy a concurrently clearing
         * START bit and request another start. */
        port->registers->CR1 =
            (port->registers->CR1 & ~(uint32_t)(I2C_CR1_POS | I2C_CR1_START)) |
            I2C_CR1_ACK;
        start_requested = false;
        if (endpoint->address_bits == 10U) {
            uint32_t header =
                0xF0U | (((uint32_t)endpoint->address >> 7U) & 6U);
            NX_STM32_I2C_WRITE_DATA(port, header);
            result = wait_flag(port, I2C_SR1_ADD10, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            NX_STM32_I2C_WRITE_DATA(port, endpoint->address & 0xFFU);
            result = wait_flag(port, I2C_SR1_ADDR, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            if (messages[message].read) {
                /* The first ADDR is transmitter-side: release it solely for
                 * the internal repeated START, without consuming payload. */
                clear_address(port);
                LL_I2C_GenerateStartCondition(port->registers);
                result = wait_flag(port, I2C_SR1_SB, deadline);
                if (result != NX_SUCCESS) {
                    break;
                }
                port->registers->CR1 &= ~(uint32_t)I2C_CR1_START;
                NX_STM32_I2C_WRITE_DATA(port, header | 1U);
            }
        } else {
            NX_STM32_I2C_WRITE_DATA(port,
                                    ((uint32_t)endpoint->address << 1U) |
                                        (messages[message].read ? 1U : 0U));
        }
        result = wait_flag(port, I2C_SR1_ADDR, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        if (messages[message].read) {
            result = receive(port, &messages[message], message + 1U == count,
                             deadline, transferred);
            start_requested = result == NX_SUCCESS && message + 1U < count;
        } else {
            clear_address(port);
            for (size_t byte = 0U; byte < messages[message].length; ++byte) {
                result = wait_flag(port, I2C_SR1_TXE, deadline);
                if (result != NX_SUCCESS) {
                    break;
                }
                NX_STM32_I2C_WRITE_DATA(port,
                                        messages[message].buffer.tx[byte]);
                result = wait_flag(port, I2C_SR1_BTF, deadline);
                if (result != NX_SUCCESS) {
                    break;
                }
                ++*transferred;
            }
        }
        if (result != NX_SUCCESS) {
            break;
        }
    }
    if (result != NX_ERROR_ARBITRATION) {
        /* A successful final receive already requested STOP in its final
         * byte window; do not issue a second STOP after master release. */
        if (result != NX_SUCCESS || !messages[count - 1U].read) {
            LL_I2C_GenerateStopCondition(port->registers);
        }
        if (!drain_stop(port) && result == NX_SUCCESS) {
            result = NX_ERROR_IO;
        }
    }
    port->registers->CR1 &= ~(uint32_t)(I2C_CR1_ACK | I2C_CR1_POS);
    port->active = false;
    if (result == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    return result;
}

/** \brief Reset only local controller; externally stuck SDA remains an error.
 */
nx_result_t nx_stm32_i2c_recover(void* context) {
    nx_stm32_i2c_state_t* port = context;
    if (port == NULL || port->registers == NULL || port->line_gpio == NULL ||
        port->line_mask == 0U || (port->line_mask & ~UINT32_C(0xFFFF)) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (!port->initialized) {
        return NX_ERROR_STATE;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    port->registers->CR1 = I2C_CR1_SWRST;
    nx_arch_dsb();
    port->registers->CR1 = 0U;
    nx_result_t result = nx_stm32_i2c_initialize(port);
    NX_STM32_IO_POLL(2U, port);
    if (result == NX_SUCCESS &&
        ((port->registers->SR2 & I2C_SR2_BUSY) != 0U ||
         !nx_stm32_i2c_lines_high(port->registers, &port->line_gpio->IDR,
                                  port->line_mask))) {
        port->fault = true;
        return NX_ERROR_IO;
    }
    return result;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_i2c_ops_t nx_stm32_i2c_ops = {
    .recover = nx_stm32_i2c_recover,
};

/** \brief Match immutable child/controller identity without MMIO or startup. */
static bool on_port(const void* context, const nx_i2c_port_t* port) {
    const nx_stm32_i2c_endpoint_state_t* endpoint = context;
    return endpoint != NULL && port != NULL && port->ops == &nx_stm32_i2c_ops &&
           port->context != NULL && endpoint->port == port->context &&
           nx_i2c_v1_address_valid(endpoint->address, endpoint->address_bits);
}

/** \brief One shared immutable method table for this execution mode. */
const nx_i2c_endpoint_ops_t nx_stm32_i2c_endpoint_ops = {
    .transaction = nx_stm32_i2c_endpoint_transaction,
    .on_port = on_port,
};
