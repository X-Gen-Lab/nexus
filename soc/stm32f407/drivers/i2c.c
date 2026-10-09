/**
 * \file            i2c.c
 * \brief           STM32F407 I2C1 standard-mode messages and bounded abort
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"

#ifndef NX_STM32_IO_POLL
#define NX_STM32_IO_POLL(kind, port) ((void)(kind), (void)(port))
#endif

/** \brief Apply only the reviewed 42 MHz / 100 kHz standard-mode profile. */
nx_result_t nx_stm32_i2c_initialize(nx_i2c_port_t* port) {
    if (port == NULL || port->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->peripheral_mhz != 42U || port->rate_hz != 100000U) {
        return NX_ERROR_UNSUPPORTED;
    }
    port->registers->CR1 = 0U;
    port->registers->CR2 = 42U;
    port->registers->CCR = 210U;
    port->registers->TRISE = 43U;
    port->registers->OAR1 = (1U << 14U);
    port->registers->CR1 = I2C_CR1_PE;
    port->active = false;
    port->fault = false;
    port->initialized = true;
    return NX_SUCCESS;
}

/** \brief Observe real status flags and retain the original hardware error. */
static nx_result_t wait_flag(nx_i2c_port_t* port, uint32_t mask,
                             nx_time_us_t deadline) {
    for (;;) {
        NX_STM32_IO_POLL(2U, port);
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
        uint32_t status = port->registers->SR1;
        if ((status & I2C_SR1_AF) != 0U) {
            port->registers->SR1 &= ~(uint32_t)I2C_SR1_AF;
            return NX_ERROR_NACK;
        }
        if ((status & I2C_SR1_ARLO) != 0U) {
            port->registers->SR1 &= ~(uint32_t)I2C_SR1_ARLO;
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
static void clear_address(nx_i2c_port_t* port) {
    (void)port->registers->SR1;
    (void)port->registers->SR2;
}

/** \brief Request STOP or the next repeated START at the final receive window.
 */
static void end_receive(nx_i2c_port_t* port, bool last) {
    port->registers->CR1 |= last ? I2C_CR1_STOP : I2C_CR1_START;
}

/** \brief Read with explicit one/two/final-three byte ACK timing. */
static nx_result_t receive(nx_i2c_port_t* port, nx_i2c_message_t* message,
                           bool last, nx_time_us_t deadline,
                           size_t* transferred) {
    I2C_TypeDef* regs = port->registers;
    size_t remaining = message->length;
    size_t index = 0U;
    nx_result_t result;
    if (remaining == 1U) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        regs->CR1 &= ~(uint32_t)I2C_CR1_ACK;
        clear_address(port);
        end_receive(port, last);
        nx_arch_irq_restore(mask);
        result = wait_flag(port, I2C_SR1_RXNE, deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        message->data[0] = (uint8_t)regs->DR;
        ++*transferred;
        return NX_SUCCESS;
    }
    if (remaining == 2U) {
        regs->CR1 |= I2C_CR1_POS;
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        clear_address(port);
        regs->CR1 &= ~(uint32_t)I2C_CR1_ACK;
        nx_arch_irq_restore(mask);
        result = wait_flag(port, I2C_SR1_BTF, deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        mask = nx_arch_irq_save();
        end_receive(port, last);
        message->data[0] = (uint8_t)regs->DR;
        message->data[1] = (uint8_t)regs->DR;
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
        message->data[index++] = (uint8_t)regs->DR;
        --remaining;
        ++*transferred;
    }
    result = wait_flag(port, I2C_SR1_BTF, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    regs->CR1 &= ~(uint32_t)I2C_CR1_ACK;
    message->data[index++] = (uint8_t)regs->DR;
    ++*transferred;
    result = wait_flag(port, I2C_SR1_BTF, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    end_receive(port, last);
    message->data[index++] = (uint8_t)regs->DR;
    message->data[index] = (uint8_t)regs->DR;
    nx_arch_irq_restore(mask);
    *transferred += 2U;
    return NX_SUCCESS;
}

/** \brief Bound STOP observation; reset local controller if the bus stays hung.
 */
static bool drain_stop(nx_i2c_port_t* port) {
    nx_time_us_t deadline = nx_deadline_after(nx_time_now_us(), 1000U);
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

/** \brief Execute a fixed-address, explicitly bounded repeated-START sequence.
 */
nx_result_t nx_i2c_endpoint_transaction(const nx_i2c_endpoint_t* endpoint,
                                        nx_i2c_message_t* messages,
                                        size_t count, nx_time_us_t deadline,
                                        size_t* transferred) {
    if (endpoint == NULL || endpoint->port == NULL ||
        endpoint->address > 0x7FU || messages == NULL || count == 0U ||
        transferred == NULL) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0U;
    if (count > 8U || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    for (size_t i = 0U; i < count; ++i) {
        if (messages[i].data == NULL || messages[i].length == 0U) {
            return NX_ERROR_INVALID;
        }
        if (messages[i].length > 256U) {
            return NX_ERROR_UNSUPPORTED;
        }
    }
    nx_i2c_port_t* port = endpoint->port;
    if (port->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active || (port->registers->SR2 & I2C_SR2_BUSY) != 0U) {
        return NX_ERROR_BUSY;
    }
    if (!port->initialized || port->fault ||
        (port->registers->CR1 & I2C_CR1_PE) == 0U) {
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    nx_result_t result = NX_SUCCESS;
    for (size_t message = 0U; message < count; ++message) {
        port->registers->CR1 = (port->registers->CR1 & ~(uint32_t)I2C_CR1_POS) |
                               I2C_CR1_ACK | I2C_CR1_START;
        result = wait_flag(port, I2C_SR1_SB, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        port->registers->DR = ((uint32_t)endpoint->address << 1U) |
                              (messages[message].read ? 1U : 0U);
        result = wait_flag(port, I2C_SR1_ADDR, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        if (messages[message].read) {
            result = receive(port, &messages[message], message + 1U == count,
                             deadline, transferred);
        } else {
            clear_address(port);
            for (size_t byte = 0U; byte < messages[message].length; ++byte) {
                result = wait_flag(port, I2C_SR1_TXE, deadline);
                if (result != NX_SUCCESS) {
                    break;
                }
                port->registers->DR = messages[message].data[byte];
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
        port->registers->CR1 |= I2C_CR1_STOP;
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
nx_result_t nx_i2c_port_recover(nx_i2c_port_t* port) {
    if (port == NULL || port->registers == NULL) {
        return NX_ERROR_INVALID;
    }
    if (nx_arch_in_isr() || nx_arch_irq_is_masked()) {
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
    if (result == NX_SUCCESS && (port->registers->SR2 & I2C_SR2_BUSY) != 0U) {
        port->fault = true;
        return NX_ERROR_IO;
    }
    return result;
}
