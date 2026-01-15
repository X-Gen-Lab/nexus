/**
 * \file            test_osal_event_properties.cpp
 * \brief           OSAL Event Flags Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Event Flags module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Maximum bits mask (24-bit support)
 */
static constexpr osal_event_bits_t MAX_BITS_MASK = 0x00FFFFFF;

/**
 * \brief           OSAL Event Flags Property Test Fixture
 */
class OsalEventPropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        osal_init();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        /* Allow cleanup */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /**
     * \brief       Generate random bits mask (1 to MAX_BITS_MASK)
     */
    osal_event_bits_t randomBitsMask() {
        std::uniform_int_distribution<uint32_t> dist(1, MAX_BITS_MASK);
        return dist(rng);
    }

    /**
     * \brief       Generate random small bits mask (1-255)
     */
    osal_event_bits_t randomSmallBitsMask() {
        std::uniform_int_distribution<uint32_t> dist(1, 255);
        return dist(rng);
    }

    /**
     * \brief       Generate random wait mode
     */
    osal_event_wait_mode_t randomWaitMode() {
        std::uniform_int_distribution<int> dist(0, 1);
        return dist(rng) == 0 ? OSAL_EVENT_WAIT_ANY : OSAL_EVENT_WAIT_ALL;
    }

    /**
     * \brief       Generate random boolean
     */
    bool randomBool() {
        std::uniform_int_distribution<int> dist(0, 1);
        return dist(rng) == 1;
    }

    /**
     * \brief       Generate random timeout (0, 100, 1000, or OSAL_WAIT_FOREVER)
     */
    uint32_t randomTimeout() {
        const uint32_t timeouts[] = {0, 100, 1000, OSAL_WAIT_FOREVER};
        std::uniform_int_distribution<size_t> dist(0, 3);
        return timeouts[dist(rng)];
    }

    /**
     * \brief       Generate random single bit (1 << n where n is 0-23)
     */
    osal_event_bits_t randomSingleBit() {
        std::uniform_int_distribution<int> dist(0, 23);
        return 1u << dist(rng);
    }
};


/*---------------------------------------------------------------------------*/
/* Property 1: Event Flags Creation Success                                  */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 1: Event Flags Creation Success
 *
 * *For any* event flags creation request, the operation SHALL succeed and
 * return a valid handle with OSAL_OK status.
 *
 * **Validates: Requirements 1.1**
 */
TEST_F(OsalEventPropertyTest, Property1_EventFlagsCreationSuccess) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        
        /* Create event flags */
        osal_status_t status = osal_event_create(&handle);
        
        /* Verify creation succeeded */
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": creation should succeed";
        EXPECT_NE(nullptr, handle)
            << "Iteration " << test_iter << ": handle should be valid";
        
        /* Clean up */
        if (handle != nullptr) {
            osal_event_delete(handle);
        }
    }
}


/*---------------------------------------------------------------------------*/
/* Property 2: Set Bits Atomically Updates State                             */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 2: Set Bits Atomically Updates State
 *
 * *For any* valid event flags handle and non-zero bits mask, setting bits
 * SHALL atomically update the event flags state such that all specified
 * bits become set.
 *
 * **Validates: Requirements 2.1, 2.5**
 */
TEST_F(OsalEventPropertyTest, Property2_SetBitsAtomicallyUpdatesState) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits mask */
        osal_event_bits_t bits_to_set = randomBitsMask();
        
        /* Set bits */
        osal_status_t status = osal_event_set(handle, bits_to_set);
        
        /* Verify set succeeded */
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": set should succeed";
        
        /* Verify all specified bits are set */
        osal_event_bits_t current_bits = osal_event_get(handle);
        EXPECT_EQ(bits_to_set, current_bits & bits_to_set)
            << "Iteration " << test_iter << ": all specified bits should be set "
            << "(expected=0x" << std::hex << bits_to_set 
            << ", got=0x" << current_bits << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 3: Set Bits Wakes Waiting Tasks                                  */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 3: Set Bits Wakes Waiting Tasks
 *
 * *For any* task waiting for event bits, setting those bits SHALL cause
 * the waiting task to unblock.
 *
 * **Validates: Requirements 2.4**
 */
TEST_F(OsalEventPropertyTest, Property3_SetBitsWakesWaitingTasks) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits to wait for */
        osal_event_bits_t bits_to_wait = randomSmallBitsMask();
        
        std::atomic<bool> task_woke_up{false};
        std::atomic<bool> task_started{false};
        
        /* Start waiting task */
        std::thread waiting_task([&]() {
            task_started = true;
            
            osal_event_wait_options_t options = {
                .mode = OSAL_EVENT_WAIT_ANY,
                .auto_clear = false,
                .timeout_ms = 5000
            };
            
            osal_status_t status = osal_event_wait(handle, bits_to_wait, &options, nullptr);
            
            if (status == OSAL_OK) {
                task_woke_up = true;
            }
        });
        
        /* Wait for task to start */
        while (!task_started) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        /* Give task time to enter wait state */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        /* Set the bits */
        osal_event_set(handle, bits_to_wait);
        
        /* Wait for task to complete */
        waiting_task.join();
        
        /* Verify task woke up */
        EXPECT_TRUE(task_woke_up)
            << "Iteration " << test_iter << ": task should wake up when bits are set "
            << "(bits=0x" << std::hex << bits_to_wait << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 4: Clear Bits Atomically Updates Only Specified Bits             */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 4: Clear Bits Atomically Updates Only Specified Bits
 *
 * *For any* valid event flags handle and non-zero bits mask, clearing bits
 * SHALL atomically clear only the specified bits while leaving all other
 * bits unchanged.
 *
 * **Validates: Requirements 3.1, 3.4, 3.5**
 */
TEST_F(OsalEventPropertyTest, Property4_ClearBitsAtomicallyUpdatesOnlySpecifiedBits) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random initial bits and bits to clear */
        osal_event_bits_t initial_bits = randomBitsMask();
        osal_event_bits_t bits_to_clear = randomBitsMask();
        
        /* Set initial bits */
        ASSERT_EQ(OSAL_OK, osal_event_set(handle, initial_bits));
        
        /* Clear some bits */
        osal_status_t status = osal_event_clear(handle, bits_to_clear);
        
        /* Verify clear succeeded */
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": clear should succeed";
        
        /* Verify only specified bits are cleared */
        osal_event_bits_t current_bits = osal_event_get(handle);
        osal_event_bits_t expected_bits = initial_bits & ~bits_to_clear;
        
        EXPECT_EQ(expected_bits, current_bits)
            << "Iteration " << test_iter << ": only specified bits should be cleared "
            << "(initial=0x" << std::hex << initial_bits 
            << ", cleared=0x" << bits_to_clear
            << ", expected=0x" << expected_bits
            << ", got=0x" << current_bits << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 5: Wait All Mode Requires All Bits                               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 5: Wait All Mode Requires All Bits
 *
 * *For any* WAIT_ALL wait operation with bits mask B, the wait SHALL
 * unblock if and only if all bits in B are set in the event flags.
 *
 * **Validates: Requirements 4.4**
 */
TEST_F(OsalEventPropertyTest, Property5_WaitAllModeRequiresAllBits) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits mask (use small mask for faster tests) */
        osal_event_bits_t bits_to_wait = randomSmallBitsMask();
        
        /* Test 1: Set all required bits - should succeed immediately */
        ASSERT_EQ(OSAL_OK, osal_event_set(handle, bits_to_wait));
        
        osal_event_wait_options_t options = {
            .mode = OSAL_EVENT_WAIT_ALL,
            .auto_clear = false,
            .timeout_ms = 100
        };
        
        osal_status_t status = osal_event_wait(handle, bits_to_wait, &options, nullptr);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": wait should succeed when all bits are set "
            << "(bits=0x" << std::hex << bits_to_wait << std::dec << ")";
        
        /* Test 2: Clear one bit - wait should timeout */
        if (bits_to_wait != 1) { /* Skip if only one bit */
            ASSERT_EQ(OSAL_OK, osal_event_clear(handle, bits_to_wait));
            
            /* Set all but one bit */
            osal_event_bits_t partial_bits = bits_to_wait & ~randomSingleBit();
            if (partial_bits != 0 && partial_bits != bits_to_wait) {
                ASSERT_EQ(OSAL_OK, osal_event_set(handle, partial_bits));
                
                options.timeout_ms = 50;
                status = osal_event_wait(handle, bits_to_wait, &options, nullptr);
                EXPECT_EQ(OSAL_ERROR_TIMEOUT, status)
                    << "Iteration " << test_iter << ": wait should timeout when not all bits are set "
                    << "(waiting=0x" << std::hex << bits_to_wait 
                    << ", set=0x" << partial_bits << std::dec << ")";
            }
        }
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 6: Wait Any Mode Requires Any Bit                                */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 6: Wait Any Mode Requires Any Bit
 *
 * *For any* WAIT_ANY wait operation with bits mask B, the wait SHALL
 * unblock if any bit in B is set in the event flags.
 *
 * **Validates: Requirements 4.5**
 */
TEST_F(OsalEventPropertyTest, Property6_WaitAnyModeRequiresAnyBit) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits mask */
        osal_event_bits_t bits_to_wait = randomSmallBitsMask();
        
        /* Set just one of the bits we're waiting for */
        osal_event_bits_t single_bit = randomSingleBit();
        while ((single_bit & bits_to_wait) == 0) {
            single_bit = randomSingleBit();
        }
        
        ASSERT_EQ(OSAL_OK, osal_event_set(handle, single_bit));
        
        /* Wait for any of the bits */
        osal_event_wait_options_t options = {
            .mode = OSAL_EVENT_WAIT_ANY,
            .auto_clear = false,
            .timeout_ms = 100
        };
        
        osal_status_t status = osal_event_wait(handle, bits_to_wait, &options, nullptr);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": wait should succeed when any bit is set "
            << "(waiting=0x" << std::hex << bits_to_wait 
            << ", set=0x" << single_bit << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 7: Auto-Clear Clears Matched Bits                                */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 7: Auto-Clear Clears Matched Bits
 *
 * *For any* wait operation with auto-clear enabled, the bits that satisfied
 * the wait condition SHALL be automatically cleared after the wait unblocks.
 *
 * **Validates: Requirements 4.6**
 */
TEST_F(OsalEventPropertyTest, Property7_AutoClearClearsMatchedBits) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits mask */
        osal_event_bits_t bits_to_wait = randomSmallBitsMask();
        osal_event_bits_t extra_bits = randomSmallBitsMask();
        
        /* Set bits to wait for plus some extra bits */
        osal_event_bits_t all_bits = bits_to_wait | extra_bits;
        ASSERT_EQ(OSAL_OK, osal_event_set(handle, all_bits));
        
        /* Wait with auto-clear enabled */
        osal_event_wait_options_t options = {
            .mode = OSAL_EVENT_WAIT_ANY,
            .auto_clear = true,
            .timeout_ms = 100
        };
        
        osal_event_bits_t bits_out = 0;
        osal_status_t status = osal_event_wait(handle, bits_to_wait, &options, &bits_out);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": wait should succeed";
        
        /* Verify matched bits are cleared */
        osal_event_bits_t current_bits = osal_event_get(handle);
        osal_event_bits_t expected_bits = all_bits & ~bits_out;
        
        EXPECT_EQ(expected_bits, current_bits)
            << "Iteration " << test_iter << ": matched bits should be cleared "
            << "(initial=0x" << std::hex << all_bits 
            << ", matched=0x" << bits_out
            << ", expected=0x" << expected_bits
            << ", got=0x" << current_bits << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 8: Non-Auto-Clear Preserves Bits                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 8: Non-Auto-Clear Preserves Bits
 *
 * *For any* wait operation with auto-clear disabled, the event bits SHALL
 * remain unchanged after the wait unblocks.
 *
 * **Validates: Requirements 4.7**
 */
TEST_F(OsalEventPropertyTest, Property8_NonAutoClearPreservesBits) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits mask */
        osal_event_bits_t bits_to_set = randomBitsMask();
        
        /* Set bits */
        ASSERT_EQ(OSAL_OK, osal_event_set(handle, bits_to_set));
        
        /* Wait without auto-clear */
        osal_event_wait_options_t options = {
            .mode = OSAL_EVENT_WAIT_ANY,
            .auto_clear = false,
            .timeout_ms = 100
        };
        
        osal_status_t status = osal_event_wait(handle, bits_to_set, &options, nullptr);
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": wait should succeed";
        
        /* Verify bits are unchanged */
        osal_event_bits_t current_bits = osal_event_get(handle);
        EXPECT_EQ(bits_to_set, current_bits & bits_to_set)
            << "Iteration " << test_iter << ": bits should be preserved "
            << "(expected=0x" << std::hex << bits_to_set 
            << ", got=0x" << current_bits << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 9: Wait Timeout Returns Error                                    */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 9: Wait Timeout Returns Error
 *
 * *For any* wait operation with timeout T, if the wait condition is not
 * satisfied within T milliseconds, the operation SHALL return OSAL_ERROR_TIMEOUT.
 *
 * **Validates: Requirements 4.8**
 */
TEST_F(OsalEventPropertyTest, Property9_WaitTimeoutReturnsError) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits to wait for (don't set them) */
        osal_event_bits_t bits_to_wait = randomSmallBitsMask();
        
        /* Use short timeout for faster tests */
        osal_event_wait_options_t options = {
            .mode = randomWaitMode(),
            .auto_clear = randomBool(),
            .timeout_ms = 50
        };
        
        auto start = std::chrono::steady_clock::now();
        osal_status_t status = osal_event_wait(handle, bits_to_wait, &options, nullptr);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        /* Verify timeout occurred */
        EXPECT_EQ(OSAL_ERROR_TIMEOUT, status)
            << "Iteration " << test_iter << ": wait should timeout "
            << "(bits=0x" << std::hex << bits_to_wait << std::dec << ")";
        
        /* Verify timeout duration is reasonable */
        EXPECT_GE(elapsed_ms, 40)
            << "Iteration " << test_iter << ": should wait at least ~50ms";
        EXPECT_LE(elapsed_ms, 200)
            << "Iteration " << test_iter << ": should not wait too long";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 10: Wait Immediate Return When Satisfied                         */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 10: Wait Immediate Return When Satisfied
 *
 * *For any* wait operation where the wait condition is already satisfied,
 * the operation SHALL return immediately with OSAL_OK without blocking.
 *
 * **Validates: Requirements 4.9**
 */
TEST_F(OsalEventPropertyTest, Property10_WaitImmediateReturnWhenSatisfied) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits and set them before waiting */
        osal_event_bits_t bits_to_wait = randomSmallBitsMask();
        ASSERT_EQ(OSAL_OK, osal_event_set(handle, bits_to_wait));
        
        /* Wait with long timeout - should return immediately */
        osal_event_wait_options_t options = {
            .mode = OSAL_EVENT_WAIT_ANY,
            .auto_clear = randomBool(),
            .timeout_ms = 5000
        };
        
        auto start = std::chrono::steady_clock::now();
        osal_status_t status = osal_event_wait(handle, bits_to_wait, &options, nullptr);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        
        /* Verify immediate return */
        EXPECT_EQ(OSAL_OK, status)
            << "Iteration " << test_iter << ": wait should succeed immediately "
            << "(bits=0x" << std::hex << bits_to_wait << std::dec << ")";
        
        EXPECT_LT(elapsed_ms, 100)
            << "Iteration " << test_iter << ": should return quickly (elapsed=" 
            << elapsed_ms << "ms)";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 11: Get Returns Current Value Without Modification               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 11: Get Returns Current Value Without Modification
 *
 * *For any* event flags state, the get operation SHALL return the current
 * bits value and SHALL not modify the event flags state.
 *
 * **Validates: Requirements 5.1, 5.3**
 */
TEST_F(OsalEventPropertyTest, Property11_GetReturnsCurrentValueWithoutModification) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits and set them */
        osal_event_bits_t bits_to_set = randomBitsMask();
        ASSERT_EQ(OSAL_OK, osal_event_set(handle, bits_to_set));
        
        /* Get bits multiple times */
        osal_event_bits_t first_get = osal_event_get(handle);
        osal_event_bits_t second_get = osal_event_get(handle);
        osal_event_bits_t third_get = osal_event_get(handle);
        
        /* Verify get returns correct value */
        EXPECT_EQ(bits_to_set, first_get & bits_to_set)
            << "Iteration " << test_iter << ": get should return current value "
            << "(expected=0x" << std::hex << bits_to_set 
            << ", got=0x" << first_get << std::dec << ")";
        
        /* Verify get doesn't modify bits */
        EXPECT_EQ(first_get, second_get)
            << "Iteration " << test_iter << ": get should not modify bits";
        EXPECT_EQ(second_get, third_get)
            << "Iteration " << test_iter << ": get should not modify bits";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 12: Set Operation Atomicity                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 12: Set Operation Atomicity
 *
 * *For any* concurrent set, clear, and wait operations on the same event
 * flags, the set operation SHALL execute atomically such that all specified
 * bits are set together without intermediate states visible to other operations.
 *
 * **Validates: Requirements 7.1**
 */
TEST_F(OsalEventPropertyTest, Property12_SetOperationAtomicity) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits for concurrent operations */
        osal_event_bits_t bits1 = randomSmallBitsMask();
        osal_event_bits_t bits2 = randomSmallBitsMask();
        
        std::atomic<int> operations_completed{0};
        
        /* Thread 1: Set bits1 */
        std::thread thread1([&]() {
            osal_event_set(handle, bits1);
            operations_completed++;
        });
        
        /* Thread 2: Set bits2 */
        std::thread thread2([&]() {
            osal_event_set(handle, bits2);
            operations_completed++;
        });
        
        /* Wait for both operations to complete */
        thread1.join();
        thread2.join();
        
        /* Verify both sets completed */
        EXPECT_EQ(2, operations_completed)
            << "Iteration " << test_iter << ": both operations should complete";
        
        /* Verify final state has both sets of bits */
        osal_event_bits_t final_bits = osal_event_get(handle);
        EXPECT_EQ(bits1, final_bits & bits1)
            << "Iteration " << test_iter << ": bits1 should be set "
            << "(bits1=0x" << std::hex << bits1 
            << ", final=0x" << final_bits << std::dec << ")";
        EXPECT_EQ(bits2, final_bits & bits2)
            << "Iteration " << test_iter << ": bits2 should be set "
            << "(bits2=0x" << std::hex << bits2 
            << ", final=0x" << final_bits << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 13: Clear Operation Atomicity                                    */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 13: Clear Operation Atomicity
 *
 * *For any* concurrent set, clear, and wait operations on the same event
 * flags, the clear operation SHALL execute atomically such that all specified
 * bits are cleared together without intermediate states visible to other operations.
 *
 * **Validates: Requirements 7.2**
 */
TEST_F(OsalEventPropertyTest, Property13_ClearOperationAtomicity) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Set initial bits */
        osal_event_bits_t initial_bits = randomBitsMask();
        ASSERT_EQ(OSAL_OK, osal_event_set(handle, initial_bits));
        
        /* Generate random bits for concurrent clear operations */
        osal_event_bits_t bits1 = randomSmallBitsMask();
        osal_event_bits_t bits2 = randomSmallBitsMask();
        
        std::atomic<int> operations_completed{0};
        
        /* Thread 1: Clear bits1 */
        std::thread thread1([&]() {
            osal_event_clear(handle, bits1);
            operations_completed++;
        });
        
        /* Thread 2: Clear bits2 */
        std::thread thread2([&]() {
            osal_event_clear(handle, bits2);
            operations_completed++;
        });
        
        /* Wait for both operations to complete */
        thread1.join();
        thread2.join();
        
        /* Verify both clears completed */
        EXPECT_EQ(2, operations_completed)
            << "Iteration " << test_iter << ": both operations should complete";
        
        /* Verify final state has both clears applied */
        osal_event_bits_t final_bits = osal_event_get(handle);
        osal_event_bits_t expected_bits = initial_bits & ~bits1 & ~bits2;
        
        EXPECT_EQ(expected_bits, final_bits)
            << "Iteration " << test_iter << ": both clears should be applied "
            << "(initial=0x" << std::hex << initial_bits 
            << ", cleared1=0x" << bits1
            << ", cleared2=0x" << bits2
            << ", expected=0x" << expected_bits
            << ", final=0x" << final_bits << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 14: Wait Check-and-Clear Atomicity                               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 14: Wait Check-and-Clear Atomicity
 *
 * *For any* wait operation with auto-clear enabled, the check of wait
 * condition and clearing of matched bits SHALL execute atomically without
 * race conditions.
 *
 * **Validates: Requirements 7.3**
 */
TEST_F(OsalEventPropertyTest, Property14_WaitCheckAndClearAtomicity) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits */
        osal_event_bits_t bits_to_wait = randomSmallBitsMask();
        
        /* Set the bits */
        ASSERT_EQ(OSAL_OK, osal_event_set(handle, bits_to_wait));
        
        std::atomic<bool> wait_completed{false};
        std::atomic<osal_event_bits_t> bits_after_wait{0};
        
        /* Thread 1: Wait with auto-clear */
        std::thread thread1([&]() {
            osal_event_wait_options_t options = {
                .mode = OSAL_EVENT_WAIT_ANY,
                .auto_clear = true,
                .timeout_ms = 1000
            };
            
            osal_event_bits_t bits_out = 0;
            osal_status_t status = osal_event_wait(handle, bits_to_wait, &options, &bits_out);
            
            if (status == OSAL_OK) {
                wait_completed = true;
                /* Read bits immediately after wait */
                bits_after_wait = osal_event_get(handle);
            }
        });
        
        /* Wait for thread to complete */
        thread1.join();
        
        /* Verify wait completed */
        EXPECT_TRUE(wait_completed)
            << "Iteration " << test_iter << ": wait should complete";
        
        /* Verify bits were cleared atomically */
        osal_event_bits_t final_bits = bits_after_wait.load();
        EXPECT_EQ(0u, final_bits & bits_to_wait)
            << "Iteration " << test_iter << ": waited bits should be cleared "
            << "(waited=0x" << std::hex << bits_to_wait 
            << ", after_wait=0x" << final_bits << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 15: Broadcast Wake All Waiting Tasks                             */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 15: Broadcast Wake All Waiting Tasks
 *
 * *For any* event flags with multiple tasks waiting for the same bits,
 * setting those bits SHALL wake all waiting tasks whose conditions are satisfied.
 *
 * **Validates: Requirements 7.4**
 */
TEST_F(OsalEventPropertyTest, Property15_BroadcastWakeAllWaitingTasks) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Generate random bits to wait for */
        osal_event_bits_t bits_to_wait = randomSmallBitsMask();
        
        const int num_waiters = 3;
        std::atomic<int> tasks_woke_up{0};
        std::atomic<int> tasks_started{0};
        std::vector<std::thread> threads;
        
        /* Start multiple waiting tasks */
        for (int i = 0; i < num_waiters; ++i) {
            threads.emplace_back([&]() {
                tasks_started++;
                
                osal_event_wait_options_t options = {
                    .mode = OSAL_EVENT_WAIT_ANY,
                    .auto_clear = false,  /* Don't clear so all can wake */
                    .timeout_ms = 5000
                };
                
                osal_status_t status = osal_event_wait(handle, bits_to_wait, &options, nullptr);
                
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
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        
        /* Set the bits - should wake all tasks */
        osal_event_set(handle, bits_to_wait);
        
        /* Wait for all tasks to complete */
        for (auto& thread : threads) {
            thread.join();
        }
        
        /* Verify all tasks woke up */
        EXPECT_EQ(num_waiters, tasks_woke_up)
            << "Iteration " << test_iter << ": all waiting tasks should wake up "
            << "(bits=0x" << std::hex << bits_to_wait << std::dec << ")";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}


/*---------------------------------------------------------------------------*/
/* Property 16: NULL Pointer Error Handling                                  */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 16: NULL Pointer Error Handling
 *
 * *For any* function that requires a non-NULL pointer parameter, passing
 * NULL SHALL return OSAL_ERROR_NULL_POINTER.
 *
 * **Validates: Requirements 8.2**
 */
TEST_F(OsalEventPropertyTest, Property16_NullPointerErrorHandling) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Test create with NULL handle */
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_create(nullptr))
            << "Iteration " << test_iter << ": create with NULL should return error";
        
        /* Test delete with NULL handle */
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_delete(nullptr))
            << "Iteration " << test_iter << ": delete with NULL should return error";
        
        /* Test set with NULL handle */
        osal_event_bits_t bits = randomBitsMask();
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_set(nullptr, bits))
            << "Iteration " << test_iter << ": set with NULL should return error";
        
        /* Test clear with NULL handle */
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_clear(nullptr, bits))
            << "Iteration " << test_iter << ": clear with NULL should return error";
        
        /* Test wait with NULL handle */
        osal_event_wait_options_t options = {
            .mode = OSAL_EVENT_WAIT_ANY,
            .auto_clear = false,
            .timeout_ms = 100
        };
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_wait(nullptr, bits, &options, nullptr))
            << "Iteration " << test_iter << ": wait with NULL should return error";
        
        /* Test get with NULL handle (returns 0, not error) */
        EXPECT_EQ(0u, osal_event_get(nullptr))
            << "Iteration " << test_iter << ": get with NULL should return 0";
        
        /* Test set_from_isr with NULL handle */
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_event_set_from_isr(nullptr, bits))
            << "Iteration " << test_iter << ": set_from_isr with NULL should return error";
    }
}


/*---------------------------------------------------------------------------*/
/* Property 17: Invalid Parameter Error Handling                             */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-event-flags, Property 17: Invalid Parameter Error Handling
 *
 * *For any* function with parameter constraints (e.g., non-zero bits mask),
 * violating those constraints SHALL return OSAL_ERROR_INVALID_PARAM.
 *
 * **Validates: Requirements 8.3**
 */
TEST_F(OsalEventPropertyTest, Property17_InvalidParameterErrorHandling) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        osal_event_handle_t handle = nullptr;
        ASSERT_EQ(OSAL_OK, osal_event_create(&handle));
        
        /* Test set with zero mask */
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_event_set(handle, 0))
            << "Iteration " << test_iter << ": set with zero mask should return error";
        
        /* Test clear with zero mask */
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_event_clear(handle, 0))
            << "Iteration " << test_iter << ": clear with zero mask should return error";
        
        /* Test wait with zero mask */
        osal_event_wait_options_t options = {
            .mode = randomWaitMode(),
            .auto_clear = randomBool(),
            .timeout_ms = 100
        };
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_event_wait(handle, 0, &options, nullptr))
            << "Iteration " << test_iter << ": wait with zero mask should return error";
        
        /* Test set_from_isr with zero mask */
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_event_set_from_isr(handle, 0))
            << "Iteration " << test_iter << ": set_from_isr with zero mask should return error";
        
        /* Clean up */
        osal_event_delete(handle);
    }
}
