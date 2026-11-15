/**
 * \file            native_spi_dma_owner_model.h
 * \brief           Observe a deterministic Native owner handoff source window
 * \author          Nexus Team
 */
#ifndef NEXUS_NATIVE_SPI_DMA_OWNER_MODEL_H
#define NEXUS_NATIVE_SPI_DMA_OWNER_MODEL_H
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Test schedules lifecycle handoff after old identity lookup. */
void nx_native_spi_dma_owner_before_step(void);
/** \brief Deliver a pending model source at actual phase publication. */
void nx_native_spi_dma_owner_after_active(void);
#ifdef __cplusplus
}
#endif
#define NX_NATIVE_SPI_DMA_BEFORE_STEP()  nx_native_spi_dma_owner_before_step()
#define NX_NATIVE_SPI_DMA_AFTER_ACTIVE() nx_native_spi_dma_owner_after_active()
#endif
