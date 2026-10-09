/**
 * \file            i2c.c
 * \brief           GD32 I2C0 limited master messages with STOP and fault drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"

static nx_i2c_port_t* s_i2c;

/** \brief           Observe errors while awaiting a protocol hardware fact. */
static nx_result_t wait_status(uint32_t mask, nx_time_us_t deadline) {
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        uint32_t flags = I2C_STAT0(I2C0);
        if ((flags & I2C_STAT0_LOSTARB) != 0u) {
            return NX_ERROR_ARBITRATION;
        }
        if ((flags & I2C_STAT0_AERR) != 0u) {
            return NX_ERROR_NACK;
        }
        if ((flags & (I2C_STAT0_BERR | I2C_STAT0_OUERR)) != 0u) {
            return NX_ERROR_IO;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
        if ((flags & mask) != 0u) {
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_IO;
}

/** \brief           Configure reviewed 100 kHz from the 50 MHz APB1 clock. */
static void configure(void) {
    I2C_CTL0(I2C0) = 0u;
    I2C_CTL1(I2C0) = 50u;
    I2C_CKCFG(I2C0) = 250u;
    I2C_RT(I2C0) = 51u;
    I2C_SADDR0(I2C0) = UINT32_C(1) << 14;
    I2C_CTL0(I2C0) = I2C_CTL0_I2CEN | I2C_CTL0_ACKEN;
}

/** \brief           Bind one explicitly wired standard-mode controller. */
nx_result_t nx_gd32_i2c_initialize(nx_i2c_port_t* port,
                                   nx_i2c_endpoint_t* endpoint, uint8_t address,
                                   uint32_t bus_hz) {
    if (!port || !endpoint || address < 8u || address > 119u) {
        return NX_ERROR_INVALID;
    }
    if (bus_hz != 100000u) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (s_i2c) {
        return NX_ERROR_BUSY;
    }
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_I2C0);
    gpio_af_set(GPIOB, GPIO_AF_4, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_6 | GPIO_PIN_7);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
    i2c_deinit(I2C0);
    configure();
    *port = (nx_i2c_port_t){.bus_hz = bus_hz, .initialized = true};
    *endpoint = (nx_i2c_endpoint_t){port, address};
    s_i2c = port;
    return NX_SUCCESS;
}

/** \brief           Clear address phase using the required two-register read.
 */
static void clear_address(void) {
    (void)I2C_STAT0(I2C0);
    (void)I2C_STAT1(I2C0);
}

/** \brief           Receive final 1/2-byte message with prearranged final NACK.
 */
static nx_result_t receive(nx_i2c_message_t* message, nx_time_us_t deadline,
                           size_t* transferred) {
    uint32_t saved = nx_gd32_critical_enter();
    I2C_CTL0(I2C0) &= ~I2C_CTL0_ACKEN;
    if (message->length == 2u) {
        I2C_CTL0(I2C0) |= I2C_CTL0_POAP;
    } else {
        I2C_CTL0(I2C0) &= ~I2C_CTL0_POAP;
    }
    clear_address();
    if (message->length == 1u) {
        I2C_CTL0(I2C0) |= I2C_CTL0_STOP;
    }
    nx_gd32_critical_leave(saved);
    nx_result_t status = wait_status(
        message->length == 1u ? I2C_STAT0_RBNE : I2C_STAT0_BTC, deadline);
    if (status != NX_SUCCESS) {
        return status;
    }
    saved = nx_gd32_critical_enter();
    if (message->length == 2u) {
        I2C_CTL0(I2C0) |= I2C_CTL0_STOP;
    }
    for (size_t i = 0u; i < message->length; ++i) {
        message->data[i] = (uint8_t)I2C_DATA(I2C0);
        ++*transferred;
    }
    nx_gd32_critical_leave(saved);
    return NX_SUCCESS;
}

/** \brief           Validate all messages before touching any buffer/hardware.
 */
nx_result_t nx_i2c_endpoint_transaction(const nx_i2c_endpoint_t* endpoint,
                                        nx_i2c_message_t* messages,
                                        size_t count, nx_time_us_t deadline,
                                        size_t* transferred) {
    if (!endpoint || !endpoint->port || endpoint->port != s_i2c || !messages ||
        !count || count > 4u || !transferred || endpoint->address < 8u ||
        endpoint->address > 119u) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0u;
    for (size_t i = 0u; i < count; ++i) {
        if (!messages[i].data || !messages[i].length ||
            messages[i].length > 256u) {
            return NX_ERROR_INVALID;
        }
        if (messages[i].read && (i + 1u != count || messages[i].length > 2u)) {
            return NX_ERROR_UNSUPPORTED;
        }
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_i2c_port_t* port = endpoint->port;
    if (!port->initialized || port->faulted) {
        return NX_ERROR_STATE;
    }
    if (port->active || (I2C_STAT1(I2C0) & I2C_STAT1_I2CBSY) != 0u) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    nx_result_t status = NX_SUCCESS;
    for (size_t i = 0u; i < count && status == NX_SUCCESS; ++i) {
        I2C_CTL0(I2C0) =
            (I2C_CTL0(I2C0) & ~I2C_CTL0_POAP) | I2C_CTL0_ACKEN | I2C_CTL0_START;
        status = wait_status(I2C_STAT0_SBSEND, deadline);
        if (status != NX_SUCCESS) {
            break;
        }
        I2C_DATA(I2C0) =
            ((uint32_t)endpoint->address << 1) | (messages[i].read ? 1u : 0u);
        status = wait_status(I2C_STAT0_ADDSEND, deadline);
        if (status != NX_SUCCESS) {
            break;
        }
        if (messages[i].read) {
            status = receive(&messages[i], deadline, transferred);
            continue;
        }
        clear_address();
        for (size_t j = 0u; j < messages[i].length; ++j) {
            status = wait_status(I2C_STAT0_TBE, deadline);
            if (status != NX_SUCCESS) {
                break;
            }
            I2C_DATA(I2C0) = messages[i].data[j];
            status = wait_status(I2C_STAT0_BTC, deadline);
            if (status != NX_SUCCESS) {
                break;
            }
            ++*transferred;
        }
        if (status == NX_SUCCESS) {
            status = wait_status(I2C_STAT0_BTC, deadline);
        }
        if (i + 1u == count && status == NX_SUCCESS) {
            I2C_CTL0(I2C0) |= I2C_CTL0_STOP;
        }
    }
    if (status != NX_ERROR_ARBITRATION &&
        (I2C_STAT1(I2C0) & I2C_STAT1_MASTER) != 0u) {
        I2C_CTL0(I2C0) |= I2C_CTL0_STOP;
    }
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        if ((I2C_CTL0(I2C0) & I2C_CTL0_STOP) == 0u) {
            break;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us()) ||
            polls + 1u == 1000000u) {
            if (status == NX_SUCCESS) {
                status = NX_ERROR_TIMEOUT;
            }
            break;
        }
    }
    if (status == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        status = NX_ERROR_TIMEOUT;
    }
    if (status != NX_SUCCESS) {
        /* A reset settles our controller; external bus idle is not inferred. */
        i2c_deinit(I2C0);
        port->faulted = true;
    } else {
        I2C_CTL0(I2C0) &= ~I2C_CTL0_POAP;
        I2C_CTL0(I2C0) |= I2C_CTL0_ACKEN;
    }
    nx_gd32_peripheral_barrier();
    port->active = false;
    if (status == NX_SUCCESS &&
        nx_deadline_expired(deadline, nx_time_now_us())) {
        status = NX_ERROR_TIMEOUT;
    }
    return status;
}

/** \brief           Reset only an idle owner and verify released line levels.
 */
nx_result_t nx_i2c_port_recover(nx_i2c_port_t* port) {
    if (!port || port != s_i2c || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    i2c_deinit(I2C0);
    configure();
    nx_gd32_peripheral_barrier();
    if ((I2C_STAT1(I2C0) & I2C_STAT1_I2CBSY) != 0u ||
        (GPIO_ISTAT(GPIOB) & (GPIO_PIN_6 | GPIO_PIN_7)) !=
            (GPIO_PIN_6 | GPIO_PIN_7)) {
        port->faulted = true;
        return NX_ERROR_IO;
    }
    port->faulted = false;
    return NX_SUCCESS;
}

/** \brief           Release our controller without claiming external bus idle.
 */
nx_result_t nx_gd32_i2c_stop(nx_i2c_port_t* port) {
    if (!port || port != s_i2c || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    i2c_deinit(I2C0);
    nx_gd32_peripheral_barrier();
    port->initialized = false;
    s_i2c = NULL;
    rcu_periph_clock_disable(RCU_I2C0);
    return NX_SUCCESS;
}
