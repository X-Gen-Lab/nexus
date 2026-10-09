/* Deterministic register model: software paths only, no HIL / timing claim. */
#ifndef NEXUS_GD32_REGISTER_MODEL_SDK_H
#define NEXUS_GD32_REGISTER_MODEL_SDK_H
#include <stdint.h>
typedef enum { RESET = 0, SET = 1 } FlagStatus;
typedef struct {
    uint32_t ISER[3], ISPR[3], IABR[3];
} NVIC_Type;
typedef struct {
    uint32_t CPACR, VTOR;
} SCB_Type;
extern NVIC_Type model_gd_nvic;
extern SCB_Type model_gd_scb;
#define NVIC         (&model_gd_nvic)
#define SCB          (&model_gd_scb)
#define TIMER1_IRQn  28
#define IPA_IRQn     90
#define SysTick_IRQn (-1)
#define PendSV_IRQn  (-2)
#define __DSB()      ((void)0)
#define __ISB()      ((void)0)
#define __NOP()      ((void)0)
/* Register bits match the locked GD SDK. SDK selectors are local model IDs. */
#define RCU_CTL_IRC16MEN        (1u << 0)
#define RCU_CTL_IRC16MSTB       (1u << 1)
#define RCU_CTL_HXTALEN         (1u << 16)
#define RCU_CTL_HXTALSTB        (1u << 17)
#define RCU_CTL_CKMEN           (1u << 19)
#define RCU_CTL_PLLEN           (1u << 24)
#define RCU_CTL_PLLSTB          (1u << 25)
#define RCU_CTL_PLLI2SEN        (1u << 26)
#define RCU_CTL_PLLI2SSTB       (1u << 27)
#define RCU_CTL_PLLSAIEN        (1u << 28)
#define RCU_CTL_PLLSAISTB       (1u << 29)
#define RCU_CFG0_SCS            0x3u
#define RCU_CFG0_SCSS           0xcu
#define RCU_CFG0_AHBPSC         0xf0u
#define RCU_CFG0_APB1PSC        0x1c00u
#define RCU_CFG0_APB2PSC        0xe000u
#define RCU_CKSYSSRC_IRC16M     0u
#define RCU_CKSYSSRC_PLLP       2u
#define RCU_SCSS_IRC16M         0u
#define RCU_SCSS_PLLP           8u
#define RCU_AHB_CKSYS_DIV1      0u
#define RCU_APB1_CKAHB_DIV4     (5u << 10)
#define RCU_APB2_CKAHB_DIV2     (4u << 13)
#define RCU_PLLSRC_HXTAL        (1u << 22)
#define RCU_TIMER_PSC_MUL2      (~(1u << 24))
#define PMU_CTL_LDOVS           (3u << 14)
#define PMU_CTL_HDEN            (1u << 16)
#define PMU_CTL_HDS             (1u << 17)
#define PMU_CS_HDRF             (1u << 16)
#define PMU_CS_HDSRF            (1u << 17)
#define RCU_PMU                 0u
#define RCU_TIMER1              1u
#define RCU_TIMER1_SLP          2u
#define RCU_APB1EN_TIMER1EN     (1u << 0)
#define RCU_APB1SPEN_TIMER1SPEN (1u << 0)
#define WS_WSCNT_7              7u
#define CK_AHB                  0u
#define CK_APB1                 1u
#define CK_APB2                 2u
enum {
    MODEL_GD_CTL,
    MODEL_GD_CFG0,
    MODEL_GD_PLL,
    MODEL_GD_PMU_CTL,
    MODEL_GD_PMU_CS,
    MODEL_GD_APB1EN,
    MODEL_GD_APB1SPEN,
    MODEL_GD_REG_COUNT
};
volatile uint32_t* model_gd_register(unsigned index);
#define RCU_CTL      (*model_gd_register(MODEL_GD_CTL))
#define RCU_CFG0     (*model_gd_register(MODEL_GD_CFG0))
#define RCU_PLL      (*model_gd_register(MODEL_GD_PLL))
#define PMU_CTL      (*model_gd_register(MODEL_GD_PMU_CTL))
#define PMU_CS       (*model_gd_register(MODEL_GD_PMU_CS))
#define RCU_APB1EN   (*model_gd_register(MODEL_GD_APB1EN))
#define RCU_APB1SPEN (*model_gd_register(MODEL_GD_APB1SPEN))
void rcu_periph_clock_enable(uint32_t);
void rcu_periph_clock_disable(uint32_t);
void rcu_periph_clock_sleep_enable(uint32_t);
void rcu_periph_clock_sleep_disable(uint32_t);
void rcu_timer_clock_prescaler_config(uint32_t);
uint32_t rcu_clock_freq_get(uint32_t);
void fmc_wscnt_set(uint32_t);
void NVIC_DisableIRQ(int);
void NVIC_EnableIRQ(int);
void NVIC_ClearPendingIRQ(int);
void NVIC_SetPriority(int, uint32_t);
#define TIMER1              1u
#define TIMER_COUNTER_EDGE  0u
#define TIMER_COUNTER_UP    0u
#define TIMER_CKDIV_DIV1    0u
#define TIMER_INT_FLAG_UP   (1u << 0)
#define TIMER_INT_UP        (1u << 0)
#define TIMER_CTL0_CEN      (1u << 0)
#define TIMER_DMAINTEN_UPIE (1u << 0)
extern uint32_t model_gd_timer_ctl0, model_gd_timer_dmainten;
extern uint32_t model_gd_timer_psc, model_gd_timer_car;
#define TIMER_CTL0(x)     (model_gd_timer_ctl0)
#define TIMER_DMAINTEN(x) (model_gd_timer_dmainten)
#define TIMER_PSC(x)      (model_gd_timer_psc)
#define TIMER_CAR(x)      (model_gd_timer_car)
typedef struct {
    uint16_t prescaler, alignedmode, counterdirection;
    uint32_t period;
    uint16_t clockdivision, repetitioncounter;
} timer_parameter_struct;
void timer_deinit(uint32_t);
void timer_struct_para_init(timer_parameter_struct*);
void timer_init(uint32_t, timer_parameter_struct*);
void timer_counter_value_config(uint32_t, uint32_t);
void timer_interrupt_flag_clear(uint32_t, uint32_t);
void timer_interrupt_enable(uint32_t, uint32_t);
void timer_enable(uint32_t);
void timer_disable(uint32_t);
uint32_t timer_counter_read(uint32_t);
FlagStatus timer_interrupt_flag_get(uint32_t, uint32_t);
extern uint32_t SystemCoreClock;
void SystemInit(void);
void SystemCoreClockUpdate(void);
#endif
