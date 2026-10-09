/**
 * \file            core_cm4.h
 * \brief           Host-only Cortex-M register model for GD32 provider faults
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_GD32_CORE_CM4_H
#define NEXUS_TEST_GD32_CORE_CM4_H
#include <stdbool.h>
#include <stdint.h>
/* Real DBG 0xE0042000 lies in ASan's shadow gap on this host. Only the
 * development register model relocates DBG; production uses the SDK address.
 */
#define NX_GD32_HOST_DBG_BASE UINT32_C(0x40040000)
#define __I                   volatile const
#define __O                   volatile
#define __IO                  volatile
#define __STATIC_INLINE       static inline
#define __NOP()               ((void)0)
#define __DSB()               ((void)0)
#define __ISB()               ((void)0)
#define __DMB()               ((void)0)
/** \brief Host interrupt state, not a physical NVIC address mapping. */
typedef struct {
    uint32_t ISER[8];
    uint32_t ICER[8];
    uint32_t ISPR[8];
    uint32_t ICPR[8];
    uint32_t IABR[8];
    uint8_t IP[240];
} NVIC_Type;
extern NVIC_Type g_gd32_model_nvic;
#define NVIC (&g_gd32_model_nvic)
/** \brief Model an enabled external interrupt bit. */
static inline void NVIC_EnableIRQ(IRQn_Type irq) {
    NVIC->ISER[(unsigned)irq / 32u] |= 1u << ((unsigned)irq % 32u);
}
/** \brief Model disabling without releasing an active handler. */
static inline void NVIC_DisableIRQ(IRQn_Type irq) {
    NVIC->ISER[(unsigned)irq / 32u] &= ~(1u << ((unsigned)irq % 32u));
}
/** \brief Clear only the selected model pending bit. */
static inline void NVIC_ClearPendingIRQ(IRQn_Type irq) {
    NVIC->ISPR[(unsigned)irq / 32u] &= ~(1u << ((unsigned)irq % 32u));
}
/** \brief Set a four-bit external preemption priority. */
static inline void NVIC_SetPriority(IRQn_Type irq, uint32_t priority) {
    NVIC->IP[(unsigned)irq] = (uint8_t)priority;
}
/** \brief Read a model preemption priority. */
static inline uint32_t NVIC_GetPriority(IRQn_Type irq) {
    return NVIC->IP[(unsigned)irq];
}
/** \brief Host-only reset CPU register storage. */
typedef struct {
    uint32_t CPACR;
    uint32_t VTOR;
    uint32_t ICSR;
} SCB_Type;
extern SCB_Type g_gd32_model_scb;
extern uint32_t g_gd32_model_mask;
extern bool g_gd32_model_isr;
#define SCB (&g_gd32_model_scb)
/** \brief Host-only scheduler tick control storage. */
typedef struct {
    uint32_t CTRL;
} SysTick_Type;
extern SysTick_Type g_gd32_model_systick;
#define SysTick                 (&g_gd32_model_systick)
#define SysTick_CTRL_ENABLE_Msk 1u
#define SCB_ICSR_PENDSTSET_Msk  (1u << 26)
#define SCB_ICSR_PENDSVSET_Msk  (1u << 28)
/** \brief Read the modeled incoming interrupt mask. */
static inline uint32_t __get_PRIMASK(void) {
    return g_gd32_model_mask;
}
/** \brief Preserve model serialization without claiming physical barriers. */
static inline void __disable_irq(void) {
    g_gd32_model_mask = 1u;
}
/** \brief Restore the modeled incoming mask. */
static inline void __set_PRIMASK(uint32_t mask) {
    g_gd32_model_mask = mask;
}
/** \brief The model does not inject a priority ceiling. */
static inline uint32_t __get_BASEPRI(void) {
    return 0u;
}
/** \brief The model does not inject FAULTMASK. */
static inline uint32_t __get_FAULTMASK(void) {
    return 0u;
}
/** \brief Read injected exception context. */
static inline uint32_t __get_IPSR(void) {
    return g_gd32_model_isr ? 16u : 0u;
}
#endif
