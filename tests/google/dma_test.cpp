/**
 * \file            dma_test.cpp
 *
 * \brief           DMA domain, physical resource and drain boundary contracts
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/dma.h"
#include <gtest/gtest.h>
#include <limits>

TEST(DmaMemory, RejectsOverflowAlignmentAndCrossDomainRanges) {
    const nx_dma_memory_region_t regions[] = {
        {0x20000000U, 0x10000U, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE},
        {0x20010000U, 0x10000U, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE},
        {0x08000000U, 0x100000U, NX_DMA_MEMORY_READ},
    };
    EXPECT_EQ(nx_dma_buffer_validate(regions, 3U,
                                     reinterpret_cast<void*>(0x20000004U), 8U,
                                     4U, NX_DMA_FROM_DEVICE),
              NX_SUCCESS);
    EXPECT_EQ(nx_dma_buffer_validate(regions, 3U,
                                     reinterpret_cast<void*>(0x10000000U), 8U,
                                     4U, NX_DMA_FROM_DEVICE),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_dma_buffer_validate(regions, 3U,
                                     reinterpret_cast<void*>(0x2000FFFCU), 8U,
                                     4U, NX_DMA_FROM_DEVICE),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_dma_buffer_validate(regions, 3U,
                                     reinterpret_cast<void*>(0x20000002U), 8U,
                                     4U, NX_DMA_FROM_DEVICE),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_dma_buffer_validate(regions, 3U,
                                     reinterpret_cast<void*>(0x08000000U), 8U,
                                     4U, NX_DMA_FROM_DEVICE),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_dma_buffer_validate(regions, 3U,
                                     reinterpret_cast<void*>(0x08000000U), 8U,
                                     4U, NX_DMA_TO_DEVICE),
              NX_SUCCESS);
    EXPECT_EQ(
        nx_dma_buffer_validate(
            regions, 3U,
            reinterpret_cast<void*>(std::numeric_limits<uintptr_t>::max() - 3U),
            8U, 4U, NX_DMA_TO_DEVICE),
        NX_ERROR_INVALID);
    const nx_dma_memory_region_t wrapped = {
        std::numeric_limits<uintptr_t>::max() - 3U, 8U, NX_DMA_MEMORY_READ};
    EXPECT_EQ(nx_dma_buffer_validate(&wrapped, 1U,
                                     reinterpret_cast<void*>(0x20000000U), 8U,
                                     4U, NX_DMA_TO_DEVICE),
              NX_ERROR_INVALID);
}

TEST(DmaResource, ChannelSelectorsDoNotCreateSeparateStreams) {
    const nx_dma_resource_t resources[] = {{1U, 3U, 2U}, {1U, 3U, 7U}};
    EXPECT_EQ(nx_dma_resources_validate(resources, 2U), NX_ERROR_BUSY);
    const nx_dma_resource_t independent[] = {{1U, 3U, 2U}, {2U, 3U, 2U}};
    EXPECT_EQ(nx_dma_resources_validate(independent, 2U), NX_SUCCESS);
    const nx_dma_resource_t invalid = {1U, 8U, 0U};
    EXPECT_EQ(nx_dma_resources_validate(&invalid, 1U), NX_ERROR_INVALID);
}

TEST(DmaDrain, CounterAndTransferCompleteNeverProveWireCompletion) {
    nx_dma_drain_facts_t facts = {8U, 0U, false, false, false};
    EXPECT_EQ(nx_dma_drain_check(&facts), NX_ERROR_BUSY);
    facts.engine_disabled = true;
    EXPECT_EQ(nx_dma_drain_check(&facts), NX_ERROR_BUSY);
    facts.irq_detached = true;
    EXPECT_EQ(nx_dma_drain_check(&facts), NX_ERROR_BUSY);
    facts.peripheral_idle = true;
    EXPECT_EQ(nx_dma_drain_check(&facts), NX_SUCCESS);
    facts.remaining = 9U;
    EXPECT_EQ(nx_dma_drain_check(&facts), NX_ERROR_INVALID);
}
