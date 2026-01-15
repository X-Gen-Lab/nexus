/**
 * \file            test_osal_event.cpp
 * \brief           OSAL Event Flags Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for OSAL Event Flags module.
 *                  Requirements: 1.1-1.6, 2.1-2.5, 3.1-3.5, 4.1-4.9, 5.1-5.4, 6.1-6.3, 8.2-8.3
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           OSAL Event Flags Test Fixture
 */
class OsalEventTest : public ::testing::Test {
  protected:
    void SetUp() override {
        osal_init();
    }

    void TearDown() override {
        /* Allow cleanup */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};

/*---------------------------------------------------------------------------*/
/* Event Flags Creation Tests - Requirements 1.1, 1.2                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test event flags creation
 * \details         Requirements 1.1 - Event flags creation should succeed
 */
TEST_F(OsalEventTest, CreateEventFlags) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));
    EXPECT_NE(nullptr, handle);

    /* Clean up */
    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test event flags creation with null handle
 * \details         Requirements 1.2 - NULL pointer should return error
 */
TEST_F(OsalEventTest, CreateWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_create(nullptr));
}

/**
 * \brief           Test creating multiple event flags
 */
TEST_F(OsalEventTest, CreateMultipleEventFlags) {
    const int num_events = 4;
    osal_event_handle_t handles[num_events];

    for (int i = 0; i < num_events; i++) {
        EXPECT_EQ(OSAL_OK, osal_event_create(&handles[i]));
        EXPECT_NE(nullptr, handles[i]);
    }

    /* Clean up */
    for (int i = 0; i < num_events; i++) {
        EXPECT_EQ(OSAL_OK, osal_event_delete(handles[i]));
    }
}

/*---------------------------------------------------------------------------*/
/* Event Flags Delete Tests - Requirements 1.5                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test event flags deletion
 * \details         Requirements 1.5 - Event flags deletion should succeed
 */
TEST_F(OsalEventTest, DeleteEventFlags) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));
    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test event flags deletion with null handle
 * \details         Requirements 1.5 - NULL handle should return error
 */
TEST_F(OsalEventTest, DeleteWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_delete(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Set Bits Tests - Requirements 2.1-2.3                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test setting event bits
 * \details         Requirements 2.1 - Setting bits should succeed
 */
TEST_F(OsalEventTest, SetBits) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x01));
    EXPECT_EQ(0x01, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x02));
    EXPECT_EQ(0x03, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test setting multiple bits at once
 */
TEST_F(OsalEventTest, SetMultipleBits) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x0F));
    EXPECT_EQ(0x0F, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test set with null handle
 * \details         Requirements 2.2 - NULL handle should return error
 */
TEST_F(OsalEventTest, SetWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_set(nullptr, 0x01));
}

/**
 * \brief           Test set with zero mask
 * \details         Requirements 2.3 - Zero mask should return error
 */
TEST_F(OsalEventTest, SetWithZeroMask) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_event_set(handle, 0x00));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Clear Bits Tests - Requirements 3.1-3.3, 3.5                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test clearing event bits
 * \details         Requirements 3.1 - Clearing bits should succeed
 */
TEST_F(OsalEventTest, ClearBits) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x0F));
    EXPECT_EQ(0x0F, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_clear(handle, 0x01));
    EXPECT_EQ(0x0E, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test clear doesn't affect other bits
 * \details         Requirements 3.5 - Clear should only affect specified bits
 */
TEST_F(OsalEventTest, ClearDoesNotAffectOtherBits) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0xFF));
    EXPECT_EQ(0xFF, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_clear(handle, 0x0F));
    EXPECT_EQ(0xF0, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test clear with null handle
 * \details         Requirements 3.2 - NULL handle should return error
 */
TEST_F(OsalEventTest, ClearWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_clear(nullptr, 0x01));
}

/**
 * \brief           Test clear with zero mask
 * \details         Requirements 3.3 - Zero mask should return error
 */
TEST_F(OsalEventTest, ClearWithZeroMask) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_event_clear(handle, 0x00));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Wait Tests - Requirements 4.1-4.9                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test wait with WAIT_ALL mode
 * \details         Requirements 4.4 - WAIT_ALL should require all bits
 */
TEST_F(OsalEventTest, WaitAllMode) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Set some bits */
    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x05)); /* bits 0 and 2 */

    /* Wait for all bits 0 and 2 - should succeed immediately */
    osal_event_wait_options_t options = {
        .mode = OSAL_EVENT_WAIT_ALL,
        .auto_clear = false,
        .timeout_ms = 100
    };
    
    osal_event_bits_t bits_out = 0;
    EXPECT_EQ(OSAL_OK, osal_event_wait(handle, 0x05, &options, &bits_out));
    EXPECT_EQ(0x05, bits_out);

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test wait with WAIT_ANY mode
 * \details         Requirements 4.5 - WAIT_ANY should require any bit
 */
TEST_F(OsalEventTest, WaitAnyMode) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Set bit 0 */
    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x01));

    /* Wait for any of bits 0, 1, or 2 - should succeed immediately */
    osal_event_wait_options_t options = {
        .mode = OSAL_EVENT_WAIT_ANY,
        .auto_clear = false,
        .timeout_ms = 100
    };
    
    osal_event_bits_t bits_out = 0;
    EXPECT_EQ(OSAL_OK, osal_event_wait(handle, 0x07, &options, &bits_out));
    EXPECT_EQ(0x01, bits_out);

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test wait with auto-clear enabled
 * \details         Requirements 4.6 - Auto-clear should clear matched bits
 */
TEST_F(OsalEventTest, WaitWithAutoClear) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Set bits */
    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x0F));

    /* Wait with auto-clear */
    osal_event_wait_options_t options = {
        .mode = OSAL_EVENT_WAIT_ANY,
        .auto_clear = true,
        .timeout_ms = 100
    };
    
    osal_event_bits_t bits_out = 0;
    EXPECT_EQ(OSAL_OK, osal_event_wait(handle, 0x03, &options, &bits_out));
    
    /* Bits 0 and 1 should be cleared */
    EXPECT_EQ(0x0C, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test wait with auto-clear disabled
 * \details         Requirements 4.7 - Non-auto-clear should preserve bits
 */
TEST_F(OsalEventTest, WaitWithoutAutoClear) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Set bits */
    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x0F));

    /* Wait without auto-clear */
    osal_event_wait_options_t options = {
        .mode = OSAL_EVENT_WAIT_ANY,
        .auto_clear = false,
        .timeout_ms = 100
    };
    
    osal_event_bits_t bits_out = 0;
    EXPECT_EQ(OSAL_OK, osal_event_wait(handle, 0x03, &options, &bits_out));
    
    /* All bits should still be set */
    EXPECT_EQ(0x0F, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test wait timeout
 * \details         Requirements 4.8 - Timeout should return error
 */
TEST_F(OsalEventTest, WaitTimeout) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Don't set any bits */

    /* Wait for bits that aren't set - should timeout */
    osal_event_wait_options_t options = {
        .mode = OSAL_EVENT_WAIT_ALL,
        .auto_clear = false,
        .timeout_ms = 100
    };
    
    auto start = std::chrono::steady_clock::now();
    osal_status_t status = osal_event_wait(handle, 0x01, &options, nullptr);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    EXPECT_EQ(OSAL_ERROR_TIMEOUT, status);
    EXPECT_GE(elapsed_ms, 80); /* Should have waited at least ~100ms */

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test wait immediate return when condition met
 * \details         Requirements 4.9 - Should return immediately if condition met
 */
TEST_F(OsalEventTest, WaitImmediateReturn) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Set bits before waiting */
    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x01));

    /* Wait should return immediately */
    osal_event_wait_options_t options = {
        .mode = OSAL_EVENT_WAIT_ANY,
        .auto_clear = false,
        .timeout_ms = 1000
    };
    
    auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(OSAL_OK, osal_event_wait(handle, 0x01, &options, nullptr));
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    EXPECT_LT(elapsed_ms, 50); /* Should return quickly */

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test wait with null handle
 * \details         Requirements 4.2 - NULL handle should return error
 */
TEST_F(OsalEventTest, WaitWithNullHandle) {
    osal_event_wait_options_t options = {
        .mode = OSAL_EVENT_WAIT_ANY,
        .auto_clear = false,
        .timeout_ms = 100
    };
    
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_wait(nullptr, 0x01, &options, nullptr));
}

/**
 * \brief           Test wait with zero mask
 * \details         Requirements 4.3 - Zero mask should return error
 */
TEST_F(OsalEventTest, WaitWithZeroMask) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    osal_event_wait_options_t options = {
        .mode = OSAL_EVENT_WAIT_ANY,
        .auto_clear = false,
        .timeout_ms = 100
    };
    
    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_event_wait(handle, 0x00, &options, nullptr));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Get Tests - Requirements 5.1-5.3                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test get returns correct value
 * \details         Requirements 5.1 - Get should return current bits
 */
TEST_F(OsalEventTest, GetReturnsCorrectValue) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Initially should be 0 */
    EXPECT_EQ(0x00, osal_event_get(handle));

    /* Set some bits */
    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0xAB));
    EXPECT_EQ(0xAB, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test get with null handle returns 0
 * \details         Requirements 5.2 - NULL handle should return 0
 */
TEST_F(OsalEventTest, GetWithNullHandle) {
    EXPECT_EQ(0x00, osal_event_get(nullptr));
}

/**
 * \brief           Test get doesn't modify bits
 * \details         Requirements 5.3 - Get should not modify bits
 */
TEST_F(OsalEventTest, GetDoesNotModifyBits) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x55));
    
    /* Get multiple times */
    EXPECT_EQ(0x55, osal_event_get(handle));
    EXPECT_EQ(0x55, osal_event_get(handle));
    EXPECT_EQ(0x55, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* ISR Tests - Requirements 6.1, 8.2, 8.3                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test set from ISR with valid parameters
 * \details         Requirements 6.1 - Set from ISR should succeed
 */
TEST_F(OsalEventTest, SetFromISR) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Note: We can't actually test ISR context in unit tests,
     * but we can test the function exists and handles valid parameters */
    EXPECT_EQ(OSAL_OK, osal_event_set_from_isr(handle, 0x01));
    EXPECT_EQ(0x01, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test set from ISR with null handle
 * \details         Requirements 8.2 - NULL handle should return error
 */
TEST_F(OsalEventTest, SetFromISRWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_set_from_isr(nullptr, 0x01));
}

/**
 * \brief           Test set from ISR with zero mask
 * \details         Requirements 8.3 - Zero mask should return error
 */
TEST_F(OsalEventTest, SetFromISRWithZeroMask) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_event_set_from_isr(handle, 0x00));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* 24-bit Support Test - Requirements 1.6                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test 24-bit support
 * \details         Requirements 1.6 - At least 24 bits should be supported
 */
TEST_F(OsalEventTest, TwentyFourBitSupport) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Set all 24 bits */
    osal_event_bits_t all_24_bits = 0x00FFFFFF;
    EXPECT_EQ(OSAL_OK, osal_event_set(handle, all_24_bits));
    
    /* Verify all 24 bits are set */
    osal_event_bits_t result = osal_event_get(handle);
    EXPECT_EQ(all_24_bits, result & all_24_bits);

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Concurrency Tests - Requirements 7.1, 7.2, 7.3, 7.4                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test concurrent set operations
 * \details         Requirements 7.1 - Multiple threads setting different bits
 *                  should all succeed atomically
 */
TEST_F(OsalEventTest, ConcurrentSetOperations) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    const int num_threads = 8;
    std::atomic<int> threads_completed{0};
    std::vector<std::thread> threads;

    /* Each thread sets a unique bit */
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            osal_event_bits_t bit = (1 << i);
            EXPECT_EQ(OSAL_OK, osal_event_set(handle, bit));
            threads_completed++;
        });
    }

    /* Wait for all threads to complete */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Verify all threads completed */
    EXPECT_EQ(num_threads, threads_completed.load());

    /* Verify all bits are set correctly */
    osal_event_bits_t expected = 0;
    for (int i = 0; i < num_threads; ++i) {
        expected |= (1 << i);
    }
    EXPECT_EQ(expected, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test concurrent clear operations
 * \details         Requirements 7.2 - Multiple threads clearing different bits
 *                  should only clear specified bits atomically
 */
TEST_F(OsalEventTest, ConcurrentClearOperations) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    /* Set all bits initially */
    const osal_event_bits_t initial_bits = 0xFFFF;
    EXPECT_EQ(OSAL_OK, osal_event_set(handle, initial_bits));

    const int num_threads = 8;
    std::atomic<int> threads_completed{0};
    std::vector<std::thread> threads;

    /* Each thread clears a unique bit */
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            osal_event_bits_t bit = (1 << i);
            EXPECT_EQ(OSAL_OK, osal_event_clear(handle, bit));
            threads_completed++;
        });
    }

    /* Wait for all threads to complete */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Verify all threads completed */
    EXPECT_EQ(num_threads, threads_completed.load());

    /* Verify only the specified bits were cleared */
    osal_event_bits_t cleared_bits = 0;
    for (int i = 0; i < num_threads; ++i) {
        cleared_bits |= (1 << i);
    }
    osal_event_bits_t expected = initial_bits & ~cleared_bits;
    EXPECT_EQ(expected, osal_event_get(handle));

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test concurrent wait operations
 * \details         Requirements 7.4 - Multiple threads waiting for overlapping
 *                  bit patterns should all wake when condition is met
 */
TEST_F(OsalEventTest, ConcurrentWaitOperations) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    const int num_waiters = 4;
    std::atomic<int> tasks_woke_up{0};
    std::atomic<int> tasks_started{0};
    std::vector<std::thread> threads;

    /* Start multiple waiting tasks - all waiting for bit 0 */
    for (int i = 0; i < num_waiters; ++i) {
        threads.emplace_back([&]() {
            tasks_started++;
            
            osal_event_wait_options_t options = {
                .mode = OSAL_EVENT_WAIT_ANY,
                .auto_clear = false,
                .timeout_ms = 2000
            };
            
            osal_status_t status = osal_event_wait(handle, 0x01, &options, nullptr);
            if (status == OSAL_OK) {
                tasks_woke_up++;
            }
        });
    }

    /* Wait for all tasks to start */
    while (tasks_started < num_waiters) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    /* Give tasks time to enter wait state */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Set bit 0 - should wake all waiting tasks */
    EXPECT_EQ(OSAL_OK, osal_event_set(handle, 0x01));

    /* Wait for all tasks to complete */
    for (auto& thread : threads) {
        thread.join();
    }

    /* Verify all tasks woke up */
    EXPECT_EQ(num_waiters, tasks_woke_up.load());

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}

/**
 * \brief           Test set/clear/wait race conditions
 * \details         Requirements 7.3 - Set and clear racing with wait operations
 *                  should have no race conditions or lost wakeups
 */
TEST_F(OsalEventTest, SetClearWaitRaceConditions) {
    osal_event_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_event_create(&handle));

    std::atomic<bool> test_running{true};
    std::atomic<int> successful_waits{0};
    std::atomic<int> set_operations{0};
    std::atomic<int> clear_operations{0};

    /* Waiter thread - repeatedly waits for bit 0 */
    std::thread waiter([&]() {
        while (test_running) {
            osal_event_wait_options_t options = {
                .mode = OSAL_EVENT_WAIT_ANY,
                .auto_clear = true,
                .timeout_ms = 100
            };
            
            osal_status_t status = osal_event_wait(handle, 0x01, &options, nullptr);
            if (status == OSAL_OK) {
                successful_waits++;
            }
        }
    });

    /* Setter thread - repeatedly sets bit 0 */
    std::thread setter([&]() {
        while (test_running) {
            osal_event_set(handle, 0x01);
            set_operations++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    /* Clearer thread - repeatedly clears bit 0 */
    std::thread clearer([&]() {
        while (test_running) {
            osal_event_clear(handle, 0x01);
            clear_operations++;
            std::this_thread::sleep_for(std::chrono::milliseconds(7));
        }
    });

    /* Let threads run for a short time */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    /* Stop all threads */
    test_running = false;

    waiter.join();
    setter.join();
    clearer.join();

    /* Verify operations occurred */
    EXPECT_GT(set_operations.load(), 0);
    EXPECT_GT(clear_operations.load(), 0);
    
    /* We should have had some successful waits (not all will succeed due to clears) */
    EXPECT_GT(successful_waits.load(), 0);

    EXPECT_EQ(OSAL_OK, osal_event_delete(handle));
}
