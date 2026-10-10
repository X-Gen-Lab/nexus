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

#ifndef NX_GD32_I2C_POLL
#define NX_GD32_I2C_POLL(port) ((void)(port))
#endif
#ifndef NX_GD32_I2C_READ_DATA
#define NX_GD32_I2C_READ_DATA(port)                                            \
    ((uint8_t)I2C_DATA((port)->controller->registers))
#endif
#ifndef NX_GD32_I2C_ADDRESS_CLEARED
#define NX_GD32_I2C_ADDRESS_CLEARED(port) ((void)(port))
#endif

const nx_gd32_i2c_controller_t nx_gd32_i2c0_controller = {I2C0, 50000000U,
                                                          RCU_I2C0};
const nx_gd32_i2c_controller_t nx_gd32_i2c1_controller = {I2C1, 50000000U,
                                                          RCU_I2C1};

/** \brief           Observe errors while awaiting a protocol hardware fact. */
static nx_result_t wait_status(nx_gd32_i2c_state_t* port, uint32_t mask,
                               nx_time_us_t deadline) {
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        NX_GD32_I2C_POLL(port);
        uint32_t flags = I2C_STAT0(port->controller->registers);
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
static void configure(nx_gd32_i2c_state_t* port) {
    I2C_CTL0(port->controller->registers) = 0u;
    I2C_CTL1(port->controller->registers) =
        port->controller->clock_hz / 1000000U;
    I2C_CKCFG(port->controller->registers) =
        port->controller->clock_hz / (2U * port->bus_hz);
    I2C_RT(port->controller->registers) =
        port->controller->clock_hz / 1000000U + 1U;
    I2C_SADDR0(port->controller->registers) = UINT32_C(1) << 14;
    I2C_CTL0(port->controller->registers) = I2C_CTL0_I2CEN | I2C_CTL0_ACKEN;
}

/** \brief Acquire selected standard-mode I2C after Board open-drain setup. */
nx_result_t nx_gd32_i2c_initialize_at(
    nx_gd32_i2c_state_t* port, nx_gd32_i2c_endpoint_state_t* endpoint,
    const nx_gd32_i2c_controller_t* controller, uint8_t address,
    uint32_t bus_hz, uint32_t line_gpio, uint32_t line_mask) {
    if (port == NULL || endpoint == NULL || address < 8U || address > 119U ||
        (controller != &nx_gd32_i2c0_controller &&
         controller != &nx_gd32_i2c1_controller) ||
        line_gpio < GPIOA || line_gpio > GPIOI ||
        (line_gpio - GPIOA) % (GPIOB - GPIOA) != 0U || line_mask == 0U ||
        (line_mask & ~UINT32_C(0xFFFF)) != 0U) {
        return NX_ERROR_INVALID;
    }
    if (bus_hz != 100000U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if ((RCU_REG_VAL(controller->clock) &
         BIT(RCU_BIT_POS(controller->clock))) != 0U) {
        return NX_ERROR_BUSY;
    }
    *port = (nx_gd32_i2c_state_t){.controller = controller,
                                  .bus_hz = bus_hz,
                                  .line_gpio = line_gpio,
                                  .line_mask = line_mask,
                                  .initialized = true};
    rcu_periph_clock_enable((rcu_periph_enum)controller->clock);
    i2c_deinit(controller->registers);
    configure(port);
    *endpoint = (nx_gd32_i2c_endpoint_state_t){port, address};
    return NX_SUCCESS;
}

/** \brief Explicit I2C0 fixture; generated boards prepare AF wiring separately.
 */
nx_result_t nx_gd32_i2c_initialize(nx_gd32_i2c_state_t* port,
                                   nx_gd32_i2c_endpoint_state_t* endpoint,
                                   uint8_t address, uint32_t bus_hz) {
    nx_result_t result = nx_gd32_i2c_initialize_at(
        port, endpoint, &nx_gd32_i2c0_controller, address, bus_hz, GPIOB,
        GPIO_PIN_6 | GPIO_PIN_7);
    if (result != NX_SUCCESS) {
        return result;
    }
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_af_set(GPIOB, GPIO_AF_4, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_6 | GPIO_PIN_7);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
    return NX_SUCCESS;
}

/** \brief           Clear address phase using the required two-register read.
 */
static void clear_address(nx_gd32_i2c_state_t* port) {
    (void)I2C_STAT0(port->controller->registers);
    (void)I2C_STAT1(port->controller->registers);
    NX_GD32_I2C_ADDRESS_CLEARED(port);
}

/** \brief Request STOP or repeated START before consuming the final pair. */
static void end_receive(nx_gd32_i2c_state_t* port, bool last) {
    I2C_CTL0(port->controller->registers) |=
        last ? I2C_CTL0_STOP : I2C_CTL0_START;
}

/** \brief Consume a bounded receive with exact 1/2/final-three-byte timing. */
static nx_result_t receive(nx_gd32_i2c_state_t* port, nx_i2c_message_t* message,
                           bool last, nx_time_us_t deadline,
                           size_t* transferred) {
    const uint32_t registers = port->controller->registers;
    size_t remaining = message->length;
    size_t index = 0u;
    nx_result_t status;
    if (remaining == 1u) {
        uint32_t saved = nx_gd32_critical_enter();
        I2C_CTL0(registers) &= ~I2C_CTL0_ACKEN;
        clear_address(port);
        end_receive(port, last);
        nx_gd32_critical_leave(saved);
        status = wait_status(port, I2C_STAT0_RBNE, deadline);
        if (status != NX_SUCCESS) {
            return status;
        }
        message->data[0] = NX_GD32_I2C_READ_DATA(port);
        ++*transferred;
        return NX_SUCCESS;
    }
    if (remaining == 2u) {
        I2C_CTL0(registers) |= I2C_CTL0_POAP;
        uint32_t saved = nx_gd32_critical_enter();
        clear_address(port);
        I2C_CTL0(registers) &= ~I2C_CTL0_ACKEN;
        nx_gd32_critical_leave(saved);
        status = wait_status(port, I2C_STAT0_BTC, deadline);
        if (status != NX_SUCCESS) {
            return status;
        }
        saved = nx_gd32_critical_enter();
        end_receive(port, last);
        message->data[0] = NX_GD32_I2C_READ_DATA(port);
        message->data[1] = NX_GD32_I2C_READ_DATA(port);
        nx_gd32_critical_leave(saved);
        *transferred += 2u;
        return NX_SUCCESS;
    }
    clear_address(port);
    while (remaining > 3u) {
        status = wait_status(port, I2C_STAT0_RBNE, deadline);
        if (status != NX_SUCCESS) {
            return status;
        }
        message->data[index++] = NX_GD32_I2C_READ_DATA(port);
        --remaining;
        ++*transferred;
    }
    status = wait_status(port, I2C_STAT0_BTC, deadline);
    if (status != NX_SUCCESS) {
        return status;
    }
    /* ACK withdrawal and N-2 consumption must share one protected window. */
    uint32_t saved = nx_gd32_critical_enter();
    I2C_CTL0(registers) &= ~I2C_CTL0_ACKEN;
    message->data[index++] = NX_GD32_I2C_READ_DATA(port);
    nx_gd32_critical_leave(saved);
    ++*transferred;
    status = wait_status(port, I2C_STAT0_BTC, deadline);
    if (status != NX_SUCCESS) {
        return status;
    }
    saved = nx_gd32_critical_enter();
    end_receive(port, last);
    message->data[index++] = NX_GD32_I2C_READ_DATA(port);
    message->data[index] = NX_GD32_I2C_READ_DATA(port);
    nx_gd32_critical_leave(saved);
    *transferred += 2u;
    return NX_SUCCESS;
}

/** \brief           Validate all messages before touching any buffer/hardware.
 */
nx_result_t nx_gd32_i2c_endpoint_transaction(void* context,
                                             nx_i2c_message_t* messages,
                                             size_t count,
                                             nx_time_us_t deadline,
                                             size_t* transferred) {
    const nx_gd32_i2c_endpoint_state_t* endpoint = context;
    if (!endpoint || !endpoint->port || endpoint->port->controller == NULL ||
        !messages || !count || !transferred || endpoint->address < 8u ||
        endpoint->address > 119u) {
        return NX_ERROR_INVALID;
    }
    *transferred = 0u;
    if (count > NX_I2C_MAX_MESSAGES || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    for (size_t i = 0u; i < count; ++i) {
        if (!messages[i].data || !messages[i].length) {
            return NX_ERROR_INVALID;
        }
        if (messages[i].length > NX_I2C_MAX_MESSAGE_BYTES) {
            return NX_ERROR_UNSUPPORTED;
        }
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    nx_gd32_i2c_state_t* port = endpoint->port;
    if (!port->initialized || port->faulted) {
        return NX_ERROR_STATE;
    }
    if (port->active ||
        (I2C_STAT1(port->controller->registers) & I2C_STAT1_I2CBSY) != 0u) {
        return NX_ERROR_BUSY;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    port->active = true;
    nx_result_t status = NX_SUCCESS;
    for (size_t i = 0u; i < count && status == NX_SUCCESS; ++i) {
        I2C_CTL0(port->controller->registers) =
            (I2C_CTL0(port->controller->registers) & ~I2C_CTL0_POAP) |
            I2C_CTL0_ACKEN | I2C_CTL0_START;
        status = wait_status(port, I2C_STAT0_SBSEND, deadline);
        if (status != NX_SUCCESS) {
            break;
        }
        I2C_DATA(port->controller->registers) =
            ((uint32_t)endpoint->address << 1) | (messages[i].read ? 1u : 0u);
        status = wait_status(port, I2C_STAT0_ADDSEND, deadline);
        if (status != NX_SUCCESS) {
            break;
        }
        if (messages[i].read) {
            status = receive(port, &messages[i], i + 1u == count, deadline,
                             transferred);
            continue;
        }
        clear_address(port);
        for (size_t j = 0u; j < messages[i].length; ++j) {
            status = wait_status(port, I2C_STAT0_TBE, deadline);
            if (status != NX_SUCCESS) {
                break;
            }
            I2C_DATA(port->controller->registers) = messages[i].data[j];
            status = wait_status(port, I2C_STAT0_BTC, deadline);
            if (status != NX_SUCCESS) {
                break;
            }
            ++*transferred;
        }
        if (status == NX_SUCCESS) {
            status = wait_status(port, I2C_STAT0_BTC, deadline);
        }
        if (i + 1u == count && status == NX_SUCCESS) {
            I2C_CTL0(port->controller->registers) |= I2C_CTL0_STOP;
        }
    }
    if (status != NX_SUCCESS && status != NX_ERROR_ARBITRATION &&
        (I2C_STAT1(port->controller->registers) & I2C_STAT1_MASTER) != 0u) {
        I2C_CTL0(port->controller->registers) |= I2C_CTL0_STOP;
    }
    for (uint32_t polls = 0u; polls < 1000000u; ++polls) {
        NX_GD32_I2C_POLL(port);
        if ((I2C_CTL0(port->controller->registers) & I2C_CTL0_STOP) == 0u &&
            (I2C_STAT1(port->controller->registers) & I2C_STAT1_MASTER) == 0u) {
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
        i2c_deinit(port->controller->registers);
        port->faulted = true;
    } else {
        I2C_CTL0(port->controller->registers) &=
            ~(I2C_CTL0_POAP | I2C_CTL0_ACKEN);
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
nx_result_t nx_gd32_i2c_recover(void* context) {
    nx_gd32_i2c_state_t* port = context;
    if (!port || port->controller == NULL || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    i2c_deinit(port->controller->registers);
    configure(port);
    nx_gd32_peripheral_barrier();
    if ((I2C_STAT1(port->controller->registers) & I2C_STAT1_I2CBSY) != 0u ||
        (GPIO_ISTAT(port->line_gpio) & port->line_mask) != port->line_mask) {
        port->faulted = true;
        return NX_ERROR_IO;
    }
    port->faulted = false;
    return NX_SUCCESS;
}

/** \brief           Release our controller without claiming external bus idle.
 */
nx_result_t nx_gd32_i2c_stop(nx_gd32_i2c_state_t* port) {
    if (!port || port->controller == NULL || !port->initialized) {
        return NX_ERROR_INVALID;
    }
    if (nx_gd32_in_isr() || nx_gd32_irq_masked()) {
        return NX_ERROR_CONTEXT;
    }
    if (port->active) {
        return NX_ERROR_BUSY;
    }
    i2c_deinit(port->controller->registers);
    nx_gd32_peripheral_barrier();
    port->initialized = false;
    rcu_periph_clock_disable((rcu_periph_enum)port->controller->clock);
    return NX_SUCCESS;
}

/** \brief One shared immutable method table for this execution mode. */
const nx_i2c_ops_t nx_gd32_i2c_ops = {
    .recover = nx_gd32_i2c_recover,
};

/** \brief One shared immutable method table for this execution mode. */
const nx_i2c_endpoint_ops_t nx_gd32_i2c_endpoint_ops = {
    .transaction = nx_gd32_i2c_endpoint_transaction,
};
