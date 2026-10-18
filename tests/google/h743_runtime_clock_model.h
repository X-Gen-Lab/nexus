/**
 * \file            h743_runtime_clock_model.h
 * \brief           External write observer for the selected lifecycle lease
 * \author          Nexus Team
 */
#ifndef NX_H743_RUNTIME_CLOCK_MODEL_H
#define NX_H743_RUNTIME_CLOCK_MODEL_H
#include "h743_spi_dma_factory_model.h"
#ifdef __cplusplus
extern "C" {
#endif
extern bool nx_runtime_watch;
extern bool nx_runtime_hold_resume_cleanup;
extern unsigned nx_runtime_unleased_writes;
void nx_runtime_register_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif
#undef NX_H743_WRITE
#define NX_H743_WRITE(reg, value) nx_runtime_register_write((reg), (value))
#undef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_runtime_register_write(&(reg), (uint32_t)(value))
#endif /* NX_H743_RUNTIME_CLOCK_MODEL_H */
