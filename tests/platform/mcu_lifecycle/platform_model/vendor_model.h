#ifndef NX_LIFECYCLE_VENDOR_MODEL_H
#define NX_LIFECYCLE_VENDOR_MODEL_H
#include <stdint.h>
#include "hal/nx_status.h"
#ifndef __weak
#define __weak __attribute__((weak))
#endif
#define __DSB() ((void)0)
#define __ISB() ((void)0)
typedef struct {
    uint32_t CTRL, LOAD, VAL;
} model_systick_t;
typedef struct {
    uint32_t ICSR;
} model_scb_t;
extern model_systick_t model_systick;
extern model_scb_t model_scb;
extern uint16_t model_flash_kib;
extern uint32_t SystemCoreClock;
#define SysTick                (&model_systick)
#define SCB                    (&model_scb)
#define FLASHSIZE_BASE         ((uintptr_t)&model_flash_kib)
#define SCB_ICSR_PENDSTCLR_Msk (1u << 25)
#define SCB_ICSR_PENDSVCLR_Msk (1u << 27)
#define SysTick_IRQn           (-1)
#define PendSV_IRQn            (-2)
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
HAL_StatusTypeDef HAL_Init(void);
HAL_StatusTypeDef HAL_DeInit(void);
uint32_t HAL_GetTick(void);
void HAL_NVIC_SetPriorityGrouping(uint32_t);
void HAL_NVIC_SetPriority(int, uint32_t, uint32_t);
void NVIC_SetPriorityGrouping(uint32_t);
void NVIC_SetPriority(int, uint32_t);
uint32_t SysTick_Config(uint32_t);
int nx_gd32f470_clock_validate(void);
int nx_gd32f470_clock_release(void);
int nx_gd32f470_timebase_init(void);
int nx_gd32f470_timebase_deinit(void);
nx_status_t nx_gd32f470_resources_idle(void);
uint32_t nx_gd32f470_millis(void);
#endif
