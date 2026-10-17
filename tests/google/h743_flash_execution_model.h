/**
 * \file            h743_flash_execution_model.h
 * \brief           External H743 write-queue and write-buffer drain model
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_EXECUTION_MODEL_H
#define NX_H743_FLASH_EXECUTION_MODEL_H
#include "h743_peripheral_model.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_h743_flash_execution_poll(void* context);
#ifdef __cplusplus
}
#endif
#undef NX_H743_FLASH_POLL
#define NX_H743_FLASH_POLL(port) nx_h743_flash_execution_poll(port)
#endif
