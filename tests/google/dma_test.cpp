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
    const nx_dma_resource_t invalid = {0U, 8U, 0U};
    EXPECT_EQ(nx_dma_resources_validate(&invalid, 1U), NX_ERROR_INVALID);
}

TEST(DmaResource, OpaqueIdentitiesDoNotImposeOneSocTopology) {
    const nx_dma_resource_t resources[] = {{3U, 31U, 255U}, {255U, 255U, 0U}};
    EXPECT_EQ(nx_dma_resources_validate(resources, 2U), NX_SUCCESS);
    const nx_dma_resource_t conflict[] = {{3U, 31U, 255U}, {3U, 31U, 0U}};
    EXPECT_EQ(nx_dma_resources_validate(conflict, 2U), NX_ERROR_BUSY);
    EXPECT_EQ(nx_dma_resources_validate(nullptr, 0U), NX_SUCCESS);
    EXPECT_EQ(nx_dma_resources_validate(nullptr, 1U), NX_ERROR_INVALID);
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

TEST(DmaStorage, WholeCacheLinesAndPayloadAreDifferentExplicitLengths) {
    alignas(32) uint8_t storage[64]{};
    const nx_dma_memory_region_t region = {
        reinterpret_cast<uintptr_t>(storage), sizeof(storage),
        NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    EXPECT_EQ(nx_dma_storage_validate(&region, 1U, storage, sizeof(storage),
                                      17U, 4U, 32U, NX_DMA_FROM_DEVICE),
              NX_SUCCESS);
    EXPECT_EQ(nx_dma_storage_validate(&region, 1U, storage, sizeof(storage),
                                      65U, 4U, 32U, NX_DMA_FROM_DEVICE),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_dma_storage_validate(&region, 1U, storage, 63U, 17U, 4U, 32U,
                                      NX_DMA_FROM_DEVICE),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_dma_storage_validate(&region, 1U, storage + 4U, 32U, 17U, 4U,
                                      32U, NX_DMA_FROM_DEVICE),
              NX_ERROR_INVALID);
    EXPECT_EQ(storage[0], 0U);
    EXPECT_EQ(storage[63], 0U);
}

TEST(DmaStorage, ValidatesTheCompleteOwnedReservationWithoutRounding) {
    const nx_dma_memory_region_t region = {
        0x20000000U, 48U, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    auto* storage = reinterpret_cast<void*>(0x20000020U);
    EXPECT_EQ(nx_dma_storage_validate(&region, 1U, storage, 32U, 4U, 4U, 32U,
                                      NX_DMA_TO_DEVICE),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_dma_storage_validate(&region, 1U, storage, 16U, 4U, 4U, 0U,
                                      NX_DMA_TO_DEVICE),
              NX_SUCCESS);
    EXPECT_EQ(nx_dma_storage_validate(&region, 1U, storage, 16U, 4U, 4U, 3U,
                                      NX_DMA_TO_DEVICE),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_dma_storage_validate(&region, 1U, storage, 16U, 0U, 4U, 0U,
                                      NX_DMA_TO_DEVICE),
              NX_ERROR_INVALID);
    const nx_dma_memory_region_t readonly = {0x20000000U, 64U,
                                             NX_DMA_MEMORY_READ};
    EXPECT_EQ(nx_dma_storage_validate(&readonly, 1U, storage, 32U, 4U, 4U, 32U,
                                      NX_DMA_FROM_DEVICE),
              NX_ERROR_PERMISSION);
}
