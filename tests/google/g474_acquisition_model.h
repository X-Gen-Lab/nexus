/**
 * \file            g474_acquisition_model.h
 * \brief           External ADC, DMA and Flash physical handshake boundary
 * \author          Nexus Team
 */
#ifndef NX_G474_ACQUISITION_MODEL_H
#define NX_G474_ACQUISITION_MODEL_H
#include "g474_model.h"
#include "stm32g474_acquisition.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_test_g474_acquisition_write(volatile uint32_t* reg, uint32_t value);
void nx_test_g474_adc_poll(nx_stm32g474_adc_state_t* port);
void nx_test_g474_flash_poll(nx_stm32g474_flash_state_t* port);
void nx_test_g474_flash_word(volatile uint32_t* address, uint32_t value);
uint32_t nx_test_g474_flash_remap(void);
#ifdef __cplusplus
}
#endif
#undef NX_G474_WRITE
#undef NX_G474_ADC_POLL
#undef NX_G474_FLASH_POLL
#undef NX_G474_FLASH_WRITE_WORD
#define NX_G474_WRITE(reg, value) nx_test_g474_acquisition_write(reg, value)
#define NX_G474_ADC_POLL(port)    nx_test_g474_adc_poll(port)
#define NX_G474_FLASH_POLL(port)  nx_test_g474_flash_poll(port)
#define NX_G474_FLASH_WRITE_WORD(address, value)                               \
    nx_test_g474_flash_word(address, value)
#define NX_G474_DMAMUX_SELECT(mux) NX_G474_WRITE(&(mux)->CCR, 5U)
#undef NX_G474_FLASH_REMAP
#define NX_G474_FLASH_REMAP() nx_test_g474_flash_remap()
#endif
