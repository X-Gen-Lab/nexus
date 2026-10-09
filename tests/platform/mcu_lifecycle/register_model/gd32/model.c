#include "model.h"
#include "arch/nx_arch.h"
#include <string.h>
volatile uint32_t model_gd_registers[MODEL_GD_REG_COUNT];
NVIC_Type model_gd_nvic;
SCB_Type model_gd_scb;
uint32_t model_gd_dma_ctl[2][8];
uint32_t model_gd_timer_ctl0, model_gd_timer_dmainten;
uint32_t model_gd_timer_psc, model_gd_timer_car, model_gd_timer_count;
uint32_t model_gd_faults, model_gd_mask;
bool model_gd_timer_pending, model_gd_unsafe_stop;
static uint32_t flash_latency, timer_clock_selector;

static void progress(void) {
    uint32_t ctl = model_gd_registers[MODEL_GD_CTL];
    uint32_t cfg = model_gd_registers[MODEL_GD_CFG0];
    if ((ctl & RCU_CTL_IRC16MEN) != 0u &&
        (model_gd_faults & MODEL_GD_IRC_START) == 0u) {
        ctl |= RCU_CTL_IRC16MSTB;
    } else { ctl &= ~RCU_CTL_IRC16MSTB; }
    if ((ctl & RCU_CTL_HXTALEN) != 0u &&
        (model_gd_faults & MODEL_GD_HXTAL_START) == 0u) {
        ctl |= RCU_CTL_HXTALSTB;
    } else if ((model_gd_faults & MODEL_GD_HXTAL_STOP) == 0u) {
        ctl &= ~RCU_CTL_HXTALSTB;
    }
    if ((ctl & RCU_CTL_PLLEN) != 0u &&
        (model_gd_faults & MODEL_GD_PLL_START) == 0u) {
        ctl |= RCU_CTL_PLLSTB;
    } else if ((model_gd_faults & MODEL_GD_PLL_STOP) == 0u) {
        ctl &= ~RCU_CTL_PLLSTB;
    }
    if ((ctl & RCU_CTL_PLLI2SEN) == 0u &&
        (model_gd_faults & MODEL_GD_PLL_STOP) == 0u) {
        ctl &= ~RCU_CTL_PLLI2SSTB;
    }
    if ((ctl & RCU_CTL_PLLSAIEN) == 0u &&
        (model_gd_faults & MODEL_GD_PLL_STOP) == 0u) {
        ctl &= ~RCU_CTL_PLLSAISTB;
    }
    if ((model_gd_faults & MODEL_GD_SWITCH) == 0u) {
        uint32_t requested = cfg & RCU_CFG0_SCS;
        if ((requested == RCU_CKSYSSRC_IRC16M &&
             (ctl & RCU_CTL_IRC16MSTB) != 0u) ||
            (requested == RCU_CKSYSSRC_PLLP &&
             (ctl & RCU_CTL_PLLSTB) != 0u)) {
            cfg = (cfg & ~RCU_CFG0_SCSS) | (requested << 2);
        }
    }
    if ((cfg & RCU_CFG0_SCSS) == RCU_SCSS_PLLP &&
        (ctl & RCU_CTL_PLLEN) == 0u) { model_gd_unsafe_stop = true; }
    uint32_t pmu = model_gd_registers[MODEL_GD_PMU_CTL];
    if ((model_gd_faults & MODEL_GD_HIGH_DRIVE) == 0u) {
        if ((pmu & PMU_CTL_HDEN) != 0u) {
            model_gd_registers[MODEL_GD_PMU_CS] |= PMU_CS_HDRF;
        }
        if ((pmu & PMU_CTL_HDS) != 0u) {
            model_gd_registers[MODEL_GD_PMU_CS] |= PMU_CS_HDSRF;
        }
    }
    model_gd_registers[MODEL_GD_CTL] = ctl;
    model_gd_registers[MODEL_GD_CFG0] = cfg;
}
volatile uint32_t* model_gd_register(unsigned index) {
    progress();
    return &model_gd_registers[index];
}
uint32_t rcu_clock_freq_get(uint32_t clock) {
    progress();
    uint32_t cfg = model_gd_registers[MODEL_GD_CFG0];
    uint32_t sys = 16000000u;
    if ((cfg & RCU_CFG0_SCSS) == RCU_SCSS_PLLP) {
        uint32_t pll = model_gd_registers[MODEL_GD_PLL];
        uint32_t m = pll & 63u, n = (pll >> 6) & 511u;
        uint32_t p = (((pll >> 16) & 3u) + 1u) * 2u;
        uint32_t input = (pll & RCU_PLLSRC_HXTAL) != 0u ? 25000000u : 16000000u;
        sys = m != 0u ? (uint32_t)(((uint64_t)input * n) / (m * p)) : 0u;
    }
    static const uint8_t shift[16] = {0,0,0,0,0,0,0,0,1,2,3,4,6,7,8,9};
    uint32_t ahb = sys >> shift[(cfg >> 4) & 15u];
    if (clock == CK_AHB) { return ahb; }
    uint32_t div = (cfg >> (clock == CK_APB1 ? 10u : 13u)) & 7u;
    return div < 4u ? ahb : (ahb >> (div - 3u));
}
void rcu_periph_clock_enable(uint32_t resource) {
    model_gd_registers[MODEL_GD_APB1EN] |= resource == RCU_TIMER1 ?
        RCU_APB1EN_TIMER1EN : (1u << 28);
}
void rcu_periph_clock_disable(uint32_t resource) {
    if ((model_gd_faults & MODEL_GD_GATE_DISABLE) == 0u && resource == RCU_TIMER1) {
        model_gd_registers[MODEL_GD_APB1EN] &= ~RCU_APB1EN_TIMER1EN;
    }
}
void rcu_periph_clock_sleep_enable(uint32_t resource) {
    if (resource == RCU_TIMER1_SLP) {
        model_gd_registers[MODEL_GD_APB1SPEN] |= RCU_APB1SPEN_TIMER1SPEN;
    }
}
void rcu_periph_clock_sleep_disable(uint32_t resource) {
    if ((model_gd_faults & MODEL_GD_GATE_DISABLE) == 0u && resource == RCU_TIMER1_SLP) {
        model_gd_registers[MODEL_GD_APB1SPEN] &= ~RCU_APB1SPEN_TIMER1SPEN;
    }
}
void rcu_timer_clock_prescaler_config(uint32_t selector) { timer_clock_selector = selector; }
void fmc_wscnt_set(uint32_t latency) { flash_latency = latency; }
void NVIC_DisableIRQ(int irq) {
    if ((model_gd_faults & MODEL_GD_IRQ_DISABLE) == 0u) {
        model_gd_nvic.ISER[(unsigned)irq / 32u] &= ~(1u << ((unsigned)irq % 32u));
    }
}
void NVIC_EnableIRQ(int irq) {
    model_gd_nvic.ISER[(unsigned)irq / 32u] |= 1u << ((unsigned)irq % 32u);
}
void NVIC_ClearPendingIRQ(int irq) {
    if ((model_gd_faults & MODEL_GD_PENDING_CLEAR) == 0u) {
        model_gd_nvic.ISPR[(unsigned)irq / 32u] &= ~(1u << ((unsigned)irq % 32u));
    }
}
void NVIC_SetPriority(int irq, uint32_t priority) { (void)irq; (void)priority; }
void timer_deinit(uint32_t timer) {
    (void)timer;model_gd_timer_ctl0=0;model_gd_timer_dmainten=0;
    model_gd_timer_psc=0;model_gd_timer_car=0;model_gd_timer_count=0;
    model_gd_timer_pending=false;
}
void timer_struct_para_init(timer_parameter_struct* p) { memset(p,0,sizeof(*p)); }
void timer_init(uint32_t timer, timer_parameter_struct* p) {
    (void)timer;model_gd_timer_psc=p->prescaler;model_gd_timer_car=p->period;
    if ((model_gd_faults & MODEL_GD_TIMER_INIT) != 0u) { ++model_gd_timer_psc; }
}
void timer_counter_value_config(uint32_t timer,uint32_t count) { (void)timer;model_gd_timer_count=count; }
void timer_interrupt_flag_clear(uint32_t timer,uint32_t flag) { (void)timer;(void)flag;model_gd_timer_pending=false; }
void timer_interrupt_enable(uint32_t timer,uint32_t flags) { (void)timer;model_gd_timer_dmainten|=flags; }
void timer_enable(uint32_t timer) { (void)timer;model_gd_timer_ctl0|=TIMER_CTL0_CEN; }
void timer_disable(uint32_t timer) {
    (void)timer;
    if ((model_gd_faults & MODEL_GD_TIMER_STOP) == 0u) { model_gd_timer_ctl0&=~TIMER_CTL0_CEN; }
}
uint32_t timer_counter_read(uint32_t timer) { (void)timer;return model_gd_timer_count; }
FlagStatus timer_interrupt_flag_get(uint32_t timer,uint32_t flag) { (void)timer;(void)flag;return model_gd_timer_pending ? SET : RESET; }
nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = {model_gd_mask};model_gd_mask=1u;return previous;
}
void nx_arch_irq_restore(nx_arch_irq_state_t previous) { model_gd_mask=previous.value; }
void model_gd_reset(void) {
    memset((void*)model_gd_registers,0,sizeof(model_gd_registers));
    memset(&model_gd_nvic,0,sizeof(model_gd_nvic));
    memset(&model_gd_scb,0,sizeof(model_gd_scb));
    memset(model_gd_dma_ctl,0,sizeof(model_gd_dma_ctl));
    model_gd_faults=0;model_gd_mask=0;model_gd_unsafe_stop=false;
    model_gd_registers[MODEL_GD_CTL]=RCU_CTL_IRC16MEN|RCU_CTL_IRC16MSTB;
    timer_deinit(TIMER1);flash_latency=7u;timer_clock_selector=0;
    SystemCoreClock=16000000u;
}
void model_gd_snapshot(model_gd_snapshot_t* out) {
    memset(out,0,sizeof(*out));
    memcpy(out->registers,(const void*)model_gd_registers,sizeof(out->registers));
    out->nvic=model_gd_nvic;out->scb=model_gd_scb;
    memcpy(out->dma,model_gd_dma_ctl,sizeof(out->dma));
    out->timer_ctl0=model_gd_timer_ctl0;out->timer_dmainten=model_gd_timer_dmainten;
    out->timer_psc=model_gd_timer_psc;out->timer_car=model_gd_timer_car;
    out->timer_count=model_gd_timer_count;out->timer_pending=model_gd_timer_pending;
    out->flash_latency=flash_latency;out->timer_clock_selector=timer_clock_selector;
}
