/**
 * \file            h743_dma_factory_model.h
 * \brief           External MMIO addresses for the generated DMA assembly
 * \author          Nexus Team
 */
#ifndef NX_H743_DMA_FACTORY_MODEL_H
#define NX_H743_DMA_FACTORY_MODEL_H
#include "h743_dma_model.h"
#include "h743_factory_model.h"
#undef DMA1
#undef DMA1_Stream0
#undef DMA1_Stream1
#undef DMAMUX1_Channel0
#undef DMAMUX1_Channel1
#define DMA1             (&g_h743_dma_registers)
#define DMA1_Stream0     (&g_h743_dma_stream)
#define DMA1_Stream1     (&g_h743_dma_stream)
#define DMAMUX1_Channel0 (&g_h743_dma_mux)
#define DMAMUX1_Channel1 (&g_h743_dma_mux)
#endif
