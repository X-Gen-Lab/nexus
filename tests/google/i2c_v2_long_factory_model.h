/**
 * \file            i2c_v2_long_factory_model.h
 * \brief           External V2 bus handshakes for normal generated factories
 * \author          Nexus Team
 */
#ifndef NX_I2C_V2_LONG_FACTORY_MODEL_H
#define NX_I2C_V2_LONG_FACTORY_MODEL_H
#if defined(NX_I2C_LONG_G474)
#include "g474_acquisition_factory_model.h"
#elif defined(NX_I2C_LONG_H743)
#include "h743_flash_image_model.h"
#include "h743_peripheral_factory_model.h"
#elif defined(NX_I2C_LONG_U575)
#include "u575_devices_model.h"
#else
#error "An exact reviewed I2C family is required"
#endif
#include "i2c_v2.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_i2c_long_factory_read(volatile uint32_t* reg);
void nx_i2c_long_factory_write(volatile uint32_t* reg, uint32_t value);
void nx_i2c_long_factory_poll(nx_i2c_v2_state_t* state);
void nx_i2c_long_factory_reset(void);
#ifdef __cplusplus
}
#include <vector>
/** \brief One accepted address phase or observed physical STOP. */
enum nx_i2c_long_factory_event_t { NX_LONG_ADDRESS, NX_LONG_STOP };
struct nx_i2c_long_control_t {
    uint32_t value;
    bool accepted;
};
struct nx_i2c_long_factory_model_t {
    nx_i2c_v2_registers_t* registers;
    volatile uint32_t* reset;
    uint32_t reset_mask;
    size_t remaining;
    size_t received_in_message;
    unsigned addresses;
    unsigned stops;
    unsigned reloads;
    unsigned resets;
    unsigned illegal_controls;
    unsigned refused_reloads;
    unsigned tcr_delay_polls;
    unsigned tcr_wait;
    bool running;
    bool pending_start;
    bool pending_tx;
    bool waiting_reload;
    bool refuse_next_reload;
    bool refuse_stop_clear;
    bool refuse_control_clear;
    std::vector<nx_i2c_long_control_t> controls;
    std::vector<uint8_t> transmitted;
    void (*event)(nx_i2c_long_factory_event_t, bool);
};
extern nx_i2c_long_factory_model_t g_long_i2c;
#endif
#undef NX_I2C_V2_READ
#undef NX_I2C_V2_WRITE
#undef NX_I2C_V2_POLL
#define NX_I2C_V2_READ(reg) nx_i2c_long_factory_read(reg)
#define NX_I2C_V2_WRITE(reg, value)                                            \
    nx_i2c_long_factory_write((reg), (uint32_t)(value))
#define NX_I2C_V2_POLL(state) nx_i2c_long_factory_poll(state)
#endif
