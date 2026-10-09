#ifndef NEXUS_GD32_REGISTER_MODEL_DMA_H
#define NEXUS_GD32_REGISTER_MODEL_DMA_H
#include "gd32f4xx.h"
extern uint32_t model_gd_dma_ctl[2][8];
#define DMA0                    0u
#define DMA1                    1u
#define DMA_CHCTL(dma, channel) (model_gd_dma_ctl[(dma)][(channel)])
#define DMA_CHXCTL_CHEN         (1u << 0)
#endif
