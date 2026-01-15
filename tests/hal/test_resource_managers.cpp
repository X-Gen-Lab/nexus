/**
 * \file            test_resource_managers.cpp
 * \brief           Resource manager verification tests (Checkpoint 4)
 * \author          Nexus Team
 *
 * Tests for:
 * - nx_dma_manager_t channel allocation/release
 * - nx_isr_manager_t multi-callback registration
 * - nx_isr_priority_t priority sorting
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "hal/nx_status.h"
#include "hal/resource/nx_dma_manager.h"
#include "hal/resource/nx_isr_manager.h"

/* Native platform provides simulation function */
void nx_isr_simulate(uint32_t irq);
}

/**
 * \brief           Test fixture for DMA manager tests
 */
class DMAManagerTest : public ::testing::Test {
  protected:
    nx_dma_manager_t* dma_mgr;

    void SetUp() override {
        dma_mgr = nx_dma_manager_get();
        ASSERT_NE(dma_mgr, nullptr);
    }

    void TearDown() override {
        /* Clean up any allocated channels */
    }
};

/**
 * \brief           Test fixture for ISR manager tests
 */
class ISRManagerTest : public ::testing::Test {
  protected:
    nx_isr_manager_t* isr_mgr;

    void SetUp() override {
        isr_mgr = nx_isr_manager_get();
        ASSERT_NE(isr_mgr, nullptr);
    }

    void TearDown() override {
        /* Clean up any registered callbacks */
    }
};

/* ========================================================================== */
/* DMA Manager Tests                                                          */
/* ========================================================================== */

/**
 * \brief           Test DMA channel allocation
 */
TEST_F(DMAManagerTest, ChannelAllocation) {
    /* Allocate a channel */
    nx_dma_channel_t* ch = dma_mgr->alloc(dma_mgr, 0x1234);
    ASSERT_NE(ch, nullptr);

    /* Free the channel */
    nx_status_t status = dma_mgr->free(dma_mgr, ch);
    EXPECT_EQ(status, NX_OK);
}

/**
 * \brief           Test multiple DMA channel allocations
 */
TEST_F(DMAManagerTest, MultipleChannelAllocation) {
    const int num_channels = 5;
    nx_dma_channel_t* channels[num_channels];

    /* Allocate multiple channels */
    for (int i = 0; i < num_channels; i++) {
        channels[i] = dma_mgr->alloc(dma_mgr, 0x1000 + i);
        ASSERT_NE(channels[i], nullptr) << "Failed to allocate channel " << i;
    }

    /* Verify all channels are different */
    for (int i = 0; i < num_channels; i++) {
        for (int j = i + 1; j < num_channels; j++) {
            EXPECT_NE(channels[i], channels[j])
                << "Channels " << i << " and " << j << " are the same";
        }
    }

    /* Free all channels */
    for (int i = 0; i < num_channels; i++) {
        nx_status_t status = dma_mgr->free(dma_mgr, channels[i]);
        EXPECT_EQ(status, NX_OK) << "Failed to free channel " << i;
    }
}

/**
 * \brief           Test DMA channel reuse after free
 */
TEST_F(DMAManagerTest, ChannelReuseAfterFree) {
    /* Allocate a channel */
    nx_dma_channel_t* ch1 = dma_mgr->alloc(dma_mgr, 0x1234);
    ASSERT_NE(ch1, nullptr);

    /* Free the channel */
    nx_status_t status = dma_mgr->free(dma_mgr, ch1);
    EXPECT_EQ(status, NX_OK);

    /* Allocate again - should succeed */
    nx_dma_channel_t* ch2 = dma_mgr->alloc(dma_mgr, 0x5678);
    ASSERT_NE(ch2, nullptr);

    /* Clean up */
    dma_mgr->free(dma_mgr, ch2);
}

/**
 * \brief           Test DMA channel exhaustion
 */
TEST_F(DMAManagerTest, ChannelExhaustion) {
    std::vector<nx_dma_channel_t*> channels;

    /* Allocate channels until exhausted */
    for (int i = 0; i < 20; i++) {
        nx_dma_channel_t* ch = dma_mgr->alloc(dma_mgr, 0x1000 + i);
        if (ch == nullptr) {
            break;
        }
        channels.push_back(ch);
    }

    /* Should have allocated at least one channel */
    EXPECT_GT(channels.size(), 0);

    /* Try to allocate one more - should fail */
    nx_dma_channel_t* ch_extra = dma_mgr->alloc(dma_mgr, 0x9999);
    EXPECT_EQ(ch_extra, nullptr);

    /* Free all channels */
    for (auto ch : channels) {
        dma_mgr->free(dma_mgr, ch);
    }
}

/**
 * \brief           Test DMA transfer start with valid parameters
 */
TEST_F(DMAManagerTest, TransferStartValid) {
    /* Allocate a channel */
    nx_dma_channel_t* ch = dma_mgr->alloc(dma_mgr, 0x1234);
    ASSERT_NE(ch, nullptr);

    /* Prepare transfer request */
    nx_dma_request_t req = {
        .periph_addr = 0x40000000,
        .memory_addr = 0x20000000,
        .transfer_count = 100,
        .periph_width = 8,
        .memory_width = 8,
        .periph_inc = false,
        .memory_inc = true,
        .circular = false,
        .priority = 2,
        .callback = nullptr,
        .user_data = nullptr,
    };

    /* Start transfer */
    nx_status_t status = dma_mgr->start(ch, &req);
    EXPECT_EQ(status, NX_OK);

    /* On native platform, non-circular transfers complete immediately */
    /* So we don't need to stop it - it's already done */

    /* Free channel */
    dma_mgr->free(dma_mgr, ch);
}

/**
 * \brief           Test DMA transfer start with invalid parameters
 */
TEST_F(DMAManagerTest, TransferStartInvalid) {
    /* Allocate a channel */
    nx_dma_channel_t* ch = dma_mgr->alloc(dma_mgr, 0x1234);
    ASSERT_NE(ch, nullptr);

    /* Test with zero transfer count */
    nx_dma_request_t req = {
        .periph_addr = 0x40000000,
        .memory_addr = 0x20000000,
        .transfer_count = 0, /* Invalid */
        .periph_width = 8,
        .memory_width = 8,
        .periph_inc = false,
        .memory_inc = true,
        .circular = false,
        .priority = 2,
        .callback = nullptr,
        .user_data = nullptr,
    };

    nx_status_t status = dma_mgr->start(ch, &req);
    EXPECT_EQ(status, NX_ERR_INVALID_PARAM);

    /* Test with invalid peripheral width */
    req.transfer_count = 100;
    req.periph_width = 7; /* Invalid */
    status = dma_mgr->start(ch, &req);
    EXPECT_EQ(status, NX_ERR_INVALID_PARAM);

    /* Test with invalid memory width */
    req.periph_width = 8;
    req.memory_width = 15; /* Invalid */
    status = dma_mgr->start(ch, &req);
    EXPECT_EQ(status, NX_ERR_INVALID_PARAM);

    /* Test with invalid priority */
    req.memory_width = 8;
    req.priority = 5; /* Invalid (max is 3) */
    status = dma_mgr->start(ch, &req);
    EXPECT_EQ(status, NX_ERR_INVALID_PARAM);

    /* Free channel */
    dma_mgr->free(dma_mgr, ch);
}

/**
 * \brief           Test DMA transfer with callback
 */
TEST_F(DMAManagerTest, TransferWithCallback) {
    static bool callback_called = false;
    static nx_status_t callback_result = NX_ERR_GENERIC;

    auto callback = [](void* user_data, nx_status_t result) {
        callback_called = true;
        callback_result = result;
        int* counter = static_cast<int*>(user_data);
        if (counter) {
            (*counter)++;
        }
    };

    int counter = 0;

    /* Allocate a channel */
    nx_dma_channel_t* ch = dma_mgr->alloc(dma_mgr, 0x1234);
    ASSERT_NE(ch, nullptr);

    /* Prepare transfer request with callback */
    nx_dma_request_t req = {
        .periph_addr = 0x40000000,
        .memory_addr = 0x20000000,
        .transfer_count = 100,
        .periph_width = 8,
        .memory_width = 8,
        .periph_inc = false,
        .memory_inc = true,
        .circular = false,
        .priority = 2,
        .callback = callback,
        .user_data = &counter,
    };

    callback_called = false;
    callback_result = NX_ERR_GENERIC;

    /* Start transfer */
    nx_status_t status = dma_mgr->start(ch, &req);
    EXPECT_EQ(status, NX_OK);

    /* On native platform, callback is called immediately for non-circular
     * transfers */
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(callback_result, NX_OK);
    EXPECT_EQ(counter, 1);

    /* Free channel */
    dma_mgr->free(dma_mgr, ch);
}

/**
 * \brief           Test DMA free with null pointer
 */
TEST_F(DMAManagerTest, FreeNullPointer) {
    nx_status_t status = dma_mgr->free(dma_mgr, nullptr);
    EXPECT_EQ(status, NX_ERR_NULL_PTR);
}

/* ========================================================================== */
/* ISR Manager Tests                                                          */
/* ========================================================================== */

/**
 * \brief           Test ISR callback registration
 */
TEST_F(ISRManagerTest, CallbackRegistration) {
    static bool callback_called = false;

    auto callback = [](void* data) { callback_called = true; };

    /* Register callback */
    nx_isr_handle_t* handle = isr_mgr->connect(isr_mgr, 10, callback, nullptr,
                                               NX_ISR_PRIORITY_NORMAL);
    ASSERT_NE(handle, nullptr);

    /* Enable IRQ */
    nx_status_t status = isr_mgr->enable(isr_mgr, 10);
    EXPECT_EQ(status, NX_OK);

    /* Simulate interrupt */
    callback_called = false;
    nx_isr_simulate(10);
    EXPECT_TRUE(callback_called);

    /* Disconnect callback */
    status = isr_mgr->disconnect(isr_mgr, handle);
    EXPECT_EQ(status, NX_OK);
}

/**
 * \brief           Test multiple ISR callback registration
 */
TEST_F(ISRManagerTest, MultipleCallbackRegistration) {
    static int callback_count = 0;

    auto callback1 = [](void* data) { callback_count++; };

    auto callback2 = [](void* data) { callback_count++; };

    auto callback3 = [](void* data) { callback_count++; };

    /* Register multiple callbacks for the same IRQ */
    nx_isr_handle_t* handle1 = isr_mgr->connect(isr_mgr, 15, callback1, nullptr,
                                                NX_ISR_PRIORITY_NORMAL);
    ASSERT_NE(handle1, nullptr);

    nx_isr_handle_t* handle2 = isr_mgr->connect(isr_mgr, 15, callback2, nullptr,
                                                NX_ISR_PRIORITY_NORMAL);
    ASSERT_NE(handle2, nullptr);

    nx_isr_handle_t* handle3 = isr_mgr->connect(isr_mgr, 15, callback3, nullptr,
                                                NX_ISR_PRIORITY_NORMAL);
    ASSERT_NE(handle3, nullptr);

    /* Enable IRQ */
    nx_status_t status = isr_mgr->enable(isr_mgr, 15);
    EXPECT_EQ(status, NX_OK);

    /* Simulate interrupt - all callbacks should be called */
    callback_count = 0;
    nx_isr_simulate(15);
    EXPECT_EQ(callback_count, 3);

    /* Disconnect all callbacks */
    isr_mgr->disconnect(isr_mgr, handle1);
    isr_mgr->disconnect(isr_mgr, handle2);
    isr_mgr->disconnect(isr_mgr, handle3);
}

/**
 * \brief           Test ISR callback priority sorting
 */
TEST_F(ISRManagerTest, CallbackPrioritySorting) {
    static std::vector<int> call_order;

    auto callback_highest = [](void* data) {
        call_order.push_back(0); /* Highest priority */
    };

    auto callback_high = [](void* data) {
        call_order.push_back(1); /* High priority */
    };

    auto callback_normal = [](void* data) {
        call_order.push_back(2); /* Normal priority */
    };

    auto callback_low = [](void* data) {
        call_order.push_back(3); /* Low priority */
    };

    /* Register callbacks in random order */
    nx_isr_handle_t* handle_normal = isr_mgr->connect(
        isr_mgr, 20, callback_normal, nullptr, NX_ISR_PRIORITY_NORMAL);
    ASSERT_NE(handle_normal, nullptr);

    nx_isr_handle_t* handle_highest = isr_mgr->connect(
        isr_mgr, 20, callback_highest, nullptr, NX_ISR_PRIORITY_HIGHEST);
    ASSERT_NE(handle_highest, nullptr);

    nx_isr_handle_t* handle_low = isr_mgr->connect(
        isr_mgr, 20, callback_low, nullptr, NX_ISR_PRIORITY_LOW);
    ASSERT_NE(handle_low, nullptr);

    nx_isr_handle_t* handle_high = isr_mgr->connect(
        isr_mgr, 20, callback_high, nullptr, NX_ISR_PRIORITY_HIGH);
    ASSERT_NE(handle_high, nullptr);

    /* Enable IRQ */
    nx_status_t status = isr_mgr->enable(isr_mgr, 20);
    EXPECT_EQ(status, NX_OK);

    /* Simulate interrupt - callbacks should be called in priority order */
    call_order.clear();
    nx_isr_simulate(20);

    /* Verify call order: HIGHEST -> HIGH -> NORMAL -> LOW */
    ASSERT_EQ(call_order.size(), 4);
    EXPECT_EQ(call_order[0], 0); /* HIGHEST */
    EXPECT_EQ(call_order[1], 1); /* HIGH */
    EXPECT_EQ(call_order[2], 2); /* NORMAL */
    EXPECT_EQ(call_order[3], 3); /* LOW */

    /* Disconnect all callbacks */
    isr_mgr->disconnect(isr_mgr, handle_highest);
    isr_mgr->disconnect(isr_mgr, handle_high);
    isr_mgr->disconnect(isr_mgr, handle_normal);
    isr_mgr->disconnect(isr_mgr, handle_low);
}

/**
 * \brief           Test ISR callback with user data
 */
TEST_F(ISRManagerTest, CallbackWithUserData) {
    static int received_value = 0;

    auto callback = [](void* data) {
        int* value = static_cast<int*>(data);
        if (value) {
            received_value = *value;
        }
    };

    int test_value = 42;

    /* Register callback with user data */
    nx_isr_handle_t* handle = isr_mgr->connect(
        isr_mgr, 25, callback, &test_value, NX_ISR_PRIORITY_NORMAL);
    ASSERT_NE(handle, nullptr);

    /* Enable IRQ */
    nx_status_t status = isr_mgr->enable(isr_mgr, 25);
    EXPECT_EQ(status, NX_OK);

    /* Simulate interrupt */
    received_value = 0;
    nx_isr_simulate(25);
    EXPECT_EQ(received_value, 42);

    /* Disconnect callback */
    isr_mgr->disconnect(isr_mgr, handle);
}

/**
 * \brief           Test ISR callback disconnection
 */
TEST_F(ISRManagerTest, CallbackDisconnection) {
    static int callback_count = 0;

    auto callback = [](void* data) { callback_count++; };

    /* Register callback */
    nx_isr_handle_t* handle = isr_mgr->connect(isr_mgr, 30, callback, nullptr,
                                               NX_ISR_PRIORITY_NORMAL);
    ASSERT_NE(handle, nullptr);

    /* Enable IRQ */
    nx_status_t status = isr_mgr->enable(isr_mgr, 30);
    EXPECT_EQ(status, NX_OK);

    /* Simulate interrupt - callback should be called */
    callback_count = 0;
    nx_isr_simulate(30);
    EXPECT_EQ(callback_count, 1);

    /* Disconnect callback */
    status = isr_mgr->disconnect(isr_mgr, handle);
    EXPECT_EQ(status, NX_OK);

    /* Simulate interrupt again - callback should NOT be called */
    callback_count = 0;
    nx_isr_simulate(30);
    EXPECT_EQ(callback_count, 0);
}

/**
 * \brief           Test ISR enable/disable
 */
TEST_F(ISRManagerTest, EnableDisable) {
    static bool callback_called = false;

    auto callback = [](void* data) { callback_called = true; };

    /* Register callback */
    nx_isr_handle_t* handle = isr_mgr->connect(isr_mgr, 35, callback, nullptr,
                                               NX_ISR_PRIORITY_NORMAL);
    ASSERT_NE(handle, nullptr);

    /* Enable IRQ */
    nx_status_t status = isr_mgr->enable(isr_mgr, 35);
    EXPECT_EQ(status, NX_OK);

    /* Simulate interrupt - callback should be called */
    callback_called = false;
    nx_isr_simulate(35);
    EXPECT_TRUE(callback_called);

    /* Disable IRQ */
    status = isr_mgr->disable(isr_mgr, 35);
    EXPECT_EQ(status, NX_OK);

    /* Simulate interrupt - callback should NOT be called */
    callback_called = false;
    nx_isr_simulate(35);
    EXPECT_FALSE(callback_called);

    /* Disconnect callback */
    isr_mgr->disconnect(isr_mgr, handle);
}

/**
 * \brief           Test ISR hardware priority setting
 */
TEST_F(ISRManagerTest, HardwarePriority) {
    /* Test valid priority */
    nx_status_t status = isr_mgr->set_hw_priority(isr_mgr, 40, 5);
    EXPECT_EQ(status, NX_OK);

    /* Test maximum valid priority */
    status = isr_mgr->set_hw_priority(isr_mgr, 40, 15);
    EXPECT_EQ(status, NX_OK);

    /* Test invalid priority (too high) */
    status = isr_mgr->set_hw_priority(isr_mgr, 40, 16);
    EXPECT_EQ(status, NX_ERR_INVALID_PARAM);
}

/**
 * \brief           Test ISR disconnect with null pointer
 */
TEST_F(ISRManagerTest, DisconnectNullPointer) {
    nx_status_t status = isr_mgr->disconnect(isr_mgr, nullptr);
    EXPECT_EQ(status, NX_ERR_NULL_PTR);
}

/**
 * \brief           Test ISR connect with null function
 */
TEST_F(ISRManagerTest, ConnectNullFunction) {
    nx_isr_handle_t* handle =
        isr_mgr->connect(isr_mgr, 45, nullptr, nullptr, NX_ISR_PRIORITY_NORMAL);
    EXPECT_EQ(handle, nullptr);
}

/**
 * \brief           Test ISR callback exhaustion
 */
TEST_F(ISRManagerTest, CallbackExhaustion) {
    std::vector<nx_isr_handle_t*> handles;

    auto callback = [](void* data) {
        /* Empty callback */
    };

    /* Register callbacks until exhausted */
    for (int i = 0; i < 10; i++) {
        nx_isr_handle_t* handle = isr_mgr->connect(
            isr_mgr, 50, callback, nullptr, NX_ISR_PRIORITY_NORMAL);
        if (handle == nullptr) {
            break;
        }
        handles.push_back(handle);
    }

    /* Should have registered at least one callback */
    EXPECT_GT(handles.size(), 0);

    /* Try to register one more - may fail if limit reached */
    nx_isr_handle_t* handle_extra = isr_mgr->connect(
        isr_mgr, 50, callback, nullptr, NX_ISR_PRIORITY_NORMAL);
    /* This may or may not be null depending on the limit */

    /* Disconnect all callbacks */
    for (auto handle : handles) {
        isr_mgr->disconnect(isr_mgr, handle);
    }
    if (handle_extra) {
        isr_mgr->disconnect(isr_mgr, handle_extra);
    }
}
