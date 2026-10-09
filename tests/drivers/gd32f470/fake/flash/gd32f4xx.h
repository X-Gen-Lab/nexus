#include "../gd32f4xx.h"
#define FMC_TOERR 3
extern uint32_t fake_fmc_ctl;
#define FMC_CTL fake_fmc_ctl
#define FMC_CTL_LK (1U<<31)
