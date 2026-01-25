/**
 * \file            test_nx_platform_properties.cpp
 * \brief           Property-Based Tests for Platform and Resource Management
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-20
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Property-based tests for platform initialization, DMA
 *                  channel management, and ISR management.
 *                  Requirements: 15.1-17.5
 */

#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <set>
#include <vector>

extern "C" {
#include "hal/base/nx_device.h"
#include "hal/nx_status.h"
#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"

/* Platform initialization functions */
nx_status_t nx_platform_init(void);
nx_status_t nx_platform_deinit(void);
bool nx_platform_is_initialized(void);

/* ISR simulation function */
void nx_isr_simulate(uint32_t irq);
}

/*---------------------------------------------------------------------------*/
/* Property Test Fixture                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Platform Property Test Fixture
 */
class PlatformPropertyTest : public ::testing::Test {
  protected:
    void SetUp() override {
        /* Seed random number generator */
        rng.seed(std::random_device{}());

        /* Ensure platform is deinitialized */
        nx_platform_deinit();
    }

    void TearDown() override {
        /* Clean up platform */
        nx_platform_deinit();
    }

    /* Random number generator */
    std::mt19937 rng;

    /* Helper: Generate random DMA index (0-1) */
    uint8_t randomDMAIndex() {
        std::uniform_int_distribution<int> dist(0, 1);
        return static_cast<uint8_t>(dist(rng));
    }

    /* Helper: Generate random channel number (0-7) */
    uint8_t randomChannel() {
        std::uniform_int_distribution<int> dist(0, 7);
        return static_cast<uint8_t>(dist(rng));
    }

    /* Helper: Generate random IRQ number (0-63) */
    uint32_t randomIRQ() {
        std::uniform_int_distribution<int> dist(0, 63);
        return static_cast<uint32_t>(dist(rng));
    }

    /* Helper: Generate random ISR priority */
    nx_isr_priority_t randomPriority() {
        std::uniform_int_distribution<int> dist(0, 3);
        return static_cast<nx_isr_priority_t>(dist(rng));
    }
};

/*---------------------------------------------------------------------------*/
/* Property 38: DMA Channel Allocation Uniqueness - Requirements 16.1        */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 38: DMA channel allocation
 * uniqueness
 *
 * *For any* DMA channel request sequence, already allocated channels should
 * not be allocated again
 *
 * **Validates: Requirements 16.1**
 */
TEST_F(PlatformPropertyTest, Property38_DMAChannelAllocationUniqueness) {
    /* Initialize platform */
    ASSERT_EQ(NX_OK, nx_platform_init());

    for (int iteration = 0; iteration < 100; ++iteration) {
        /* Generate random number of channels to allocate (1-8) */
        std::uniform_int_distribution<int> count_dist(1, 8);
        int num_channels = count_dist(rng);

        /* Track allocated channels */
        std::vector<nx_dma_channel_t*> allocated_channels;
        std::set<std::pair<uint8_t, uint8_t>> allocated_ids;

        /* Allocate channels */
        for (int i = 0; i < num_channels; ++i) {
            uint8_t dma_idx = randomDMAIndex();
            uint8_t channel = randomChannel();

            nx_dma_channel_t* ch = nx_dma_allocate_channel(dma_idx, channel);

            if (ch != nullptr) {
                /* Channel was allocated */
                allocated_channels.push_back(ch);

                /* This DMA/channel combination should not have been allocated
                 * before */
                auto id_pair = std::make_pair(dma_idx, channel);
                EXPECT_EQ(allocated_ids.count(id_pair), 0U)
                    << "Channel " << static_cast<int>(channel) << " on DMA "
                    << static_cast<int>(dma_idx)
                    << " was allocated twice in iteration " << iteration;

                allocated_ids.insert(id_pair);
            } else {
                /* Channel allocation failed - it should already be allocated */
                auto id_pair = std::make_pair(dma_idx, channel);
                EXPECT_GT(allocated_ids.count(id_pair), 0U)
                    << "Channel " << static_cast<int>(channel) << " on DMA "
                    << static_cast<int>(dma_idx)
                    << " allocation failed but was not previously allocated in "
                       "iteration "
                    << iteration;
            }
        }

        /* Verify all allocated channels are unique */
        EXPECT_EQ(allocated_channels.size(), allocated_ids.size())
            << "Number of allocated channels does not match unique IDs in "
               "iteration "
            << iteration;

        /* Release all allocated channels */
        for (auto* ch : allocated_channels) {
            EXPECT_EQ(NX_OK, nx_dma_release_channel(ch));
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Property 39: DMA Channel Release Availability - Requirements 16.5         */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 39: DMA channel release
 * availability
 *
 * *For any* DMA channel, after releasing it should be available for
 * reallocation
 *
 * **Validates: Requirements 16.5**
 */
TEST_F(PlatformPropertyTest, Property39_DMAChannelReleaseAvailability) {
    /* Initialize platform */
    ASSERT_EQ(NX_OK, nx_platform_init());

    for (int iteration = 0; iteration < 100; ++iteration) {
        /* Generate random DMA and channel */
        uint8_t dma_idx = randomDMAIndex();
        uint8_t channel = randomChannel();

        /* Allocate channel */
        nx_dma_channel_t* ch1 = nx_dma_allocate_channel(dma_idx, channel);
        ASSERT_NE(nullptr, ch1)
            << "Failed to allocate channel " << static_cast<int>(channel)
            << " on DMA " << static_cast<int>(dma_idx) << " in iteration "
            << iteration;

        /* Try to allocate same channel again - should fail */
        nx_dma_channel_t* ch2 = nx_dma_allocate_channel(dma_idx, channel);
        EXPECT_EQ(nullptr, ch2)
            << "Channel " << static_cast<int>(channel) << " on DMA "
            << static_cast<int>(dma_idx)
            << " was allocated twice before release in iteration " << iteration;

        /* Release channel */
        EXPECT_EQ(NX_OK, nx_dma_release_channel(ch1))
            << "Failed to release channel " << static_cast<int>(channel)
            << " on DMA " << static_cast<int>(dma_idx) << " in iteration "
            << iteration;

        /* Allocate same channel again - should succeed */
        nx_dma_channel_t* ch3 = nx_dma_allocate_channel(dma_idx, channel);
        EXPECT_NE(nullptr, ch3)
            << "Failed to reallocate channel " << static_cast<int>(channel)
            << " on DMA " << static_cast<int>(dma_idx)
            << " after release in iteration " << iteration;

        /* Release channel */
        if (ch3 != nullptr) {
            EXPECT_EQ(NX_OK, nx_dma_release_channel(ch3));
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Property 40: ISR Registration Trigger Consistency - Requirements 17.1,    */
/* 17.2                                                                       */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property 40: ISR registration trigger
 * consistency
 *
 * *For any* ISR handler function, after registering and enabling, triggering
 * the interrupt should call the handler
 *
 * **Validates: Requirements 17.1, 17.2**
 */
TEST_F(PlatformPropertyTest, Property40_ISRRegistrationTriggerConsistency) {
    /* Initialize platform */
    ASSERT_EQ(NX_OK, nx_platform_init());

    /* Get ISR manager */
    nx_isr_manager_t* isr_mgr = nx_isr_manager_get();
    ASSERT_NE(nullptr, isr_mgr);

    for (int iteration = 0; iteration < 100; ++iteration) {
        /* Generate random IRQ */
        uint32_t irq = randomIRQ();

        /* Generate random priority */
        nx_isr_priority_t priority = randomPriority();

        /* Create handler with call counter */
        int call_count = 0;
        auto handler = [](void* user_data) {
            int* count = static_cast<int*>(user_data);
            (*count)++;
        };

        /* Register handler */
        nx_isr_handle_t* handle =
            isr_mgr->connect(isr_mgr, irq, handler, &call_count, priority);

        ASSERT_NE(nullptr, handle) << "Failed to register ISR for IRQ " << irq
                                   << " in iteration " << iteration;

        /* Enable interrupt */
        EXPECT_EQ(NX_OK, isr_mgr->enable(isr_mgr, irq))
            << "Failed to enable IRQ " << irq << " in iteration " << iteration;

        /* Simulate interrupt */
        nx_isr_simulate(irq);

        /* Handler should have been called exactly once */
        EXPECT_EQ(1, call_count)
            << "Handler for IRQ " << irq << " was called " << call_count
            << " times (expected 1) in iteration " << iteration;

        /* Simulate interrupt again */
        nx_isr_simulate(irq);

        /* Handler should have been called twice total */
        EXPECT_EQ(2, call_count)
            << "Handler for IRQ " << irq << " was called " << call_count
            << " times (expected 2) after second trigger in iteration "
            << iteration;

        /* Disconnect handler */
        EXPECT_EQ(NX_OK, isr_mgr->disconnect(isr_mgr, handle))
            << "Failed to disconnect ISR for IRQ " << irq << " in iteration "
            << iteration;

        /* Simulate interrupt again */
        nx_isr_simulate(irq);

        /* Handler should NOT have been called again (still 2) */
        EXPECT_EQ(2, call_count)
            << "Handler for IRQ " << irq << " was called " << call_count
            << " times (expected 2) after disconnect in iteration "
            << iteration;
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property: DMA Configuration Persistence                        */
/*---------------------------------------------------------------------------*/

/**
 * Feature: native-hal-validation, Property: DMA configuration persistence
 *
 * *For any* DMA channel configuration, the configuration should persist until
 * the channel is released
 *
 * **Validates: Requirements 16.2**
 */
TEST_F(PlatformPropertyTest, PropertyExtra_DMAConfigurationPersistence) {
    /* Initialize platform */
    ASSERT_EQ(NX_OK, nx_platform_init());

    for (int iteration = 0; iteration < 100; ++iteration) {
        /* Generate random DMA and channel */
        uint8_t dma_idx = randomDMAIndex();
        uint8_t channel = randomChannel();

        /* Allocate channel */
        nx_dma_channel_t* ch = nx_dma_allocate_channel(dma_idx, channel);
        ASSERT_NE(nullptr, ch)
            << "Failed to allocate channel in iteration " << iteration;

        /* Generate random configuration */
        std::uniform_int_distribution<int> size_dist(1, 1024);
        size_t transfer_size = static_cast<size_t>(size_dist(rng));

        std::uniform_int_distribution<int> width_dist(0, 2);
        uint8_t data_widths[] = {1, 2, 4};
        uint8_t data_width = data_widths[width_dist(rng)];

        std::uniform_int_distribution<int> bool_dist(0, 1);
        bool src_inc = bool_dist(rng) == 1;
        bool dst_inc = bool_dist(rng) == 1;
        bool circular = bool_dist(rng) == 1;

        /* Create dummy buffers */
        static uint8_t src_buffer[1024];
        static uint8_t dst_buffer[1024];

        nx_dma_config_t config = {
            .src_addr = src_buffer,
            .dst_addr = dst_buffer,
            .size = transfer_size,
            .src_inc = src_inc ? (uint8_t)1 : (uint8_t)0,
            .dst_inc = dst_inc ? (uint8_t)1 : (uint8_t)0,
            .data_width = data_width,
            .circular = circular,
        };

        /* Configure channel */
        EXPECT_EQ(NX_OK, ch->configure(ch, &config))
            << "Failed to configure channel in iteration " << iteration;

        /* Configuration should persist - we can't directly verify internal
         * state, but we can verify that operations work correctly */

        /* For non-circular mode, start should complete immediately */
        if (!circular) {
            EXPECT_EQ(NX_OK, ch->start(ch))
                << "Failed to start transfer in iteration " << iteration;

            /* Remaining should be 0 after completion */
            EXPECT_EQ(0U, ch->get_remaining(ch))
                << "Remaining count should be 0 after non-circular transfer in "
                   "iteration "
                << iteration;
        }

        /* Release channel */
        EXPECT_EQ(NX_OK, nx_dma_release_channel(ch))
            << "Failed to release channel in iteration " << iteration;
    }
}
