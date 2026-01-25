/**
 * \file            test_osal_queue_properties.cpp
 * \brief           OSAL Queue Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for OSAL Queue module.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
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
 * \brief           OSAL Queue Property Test Fixture
 */
class OsalQueuePropertyTest : public ::testing::Test {
  protected:
    std::mt19937 rng;

    void SetUp() override {
        osal_init();
        rng.seed(std::random_device{}());
    }

    void TearDown() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    /**
     * \brief       Generate random queue capacity (2-20)
     */
    size_t randomCapacity() {
        std::uniform_int_distribution<size_t> dist(2, 20);
        return dist(rng);
    }

    /**
     * \brief       Generate random number of items to send (1-capacity)
     */
    size_t randomItemCount(size_t max_count) {
        std::uniform_int_distribution<size_t> dist(1, max_count);
        return dist(rng);
    }

    /**
     * \brief       Generate random integer value
     */
    int randomValue() {
        std::uniform_int_distribution<int> dist(-10000, 10000);
        return dist(rng);
    }

    /**
     * \brief       Generate random byte value
     */
    uint8_t randomByte() {
        std::uniform_int_distribution<int> dist(0, 255);
        return static_cast<uint8_t>(dist(rng));
    }
};

/*---------------------------------------------------------------------------*/
/* Property 9: Queue Round-Trip Consistency                                  */
/* Feature: freertos-adapter, Property 9: Queue Round-Trip Consistency       */
/*---------------------------------------------------------------------------*/

/**
 * Feature: freertos-adapter, Property 9: Queue Round-Trip Consistency
 *
 * *For any* queue with item_size S and item_count N, sending an item then
 * receiving SHALL return an item with identical content to what was sent.
 *
 * **Validates: Requirements 7.1, 7.3, 7.5**
 */
TEST_F(OsalQueuePropertyTest, Property9_QueueRoundTripConsistency) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random item size (1-64 bytes) */
        std::uniform_int_distribution<size_t> size_dist(1, 64);
        size_t item_size = size_dist(rng);
        size_t capacity = randomCapacity();

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(item_size, capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed "
            << "(item_size=" << item_size << ", capacity=" << capacity << ")";

        /* Generate random data to send */
        std::vector<uint8_t> send_data(item_size);
        for (size_t i = 0; i < item_size; i++) {
            send_data[i] = randomByte();
        }

        /* Send the item */
        ASSERT_EQ(OSAL_OK,
                  osal_queue_send(queue, send_data.data(), OSAL_NO_WAIT))
            << "Iteration " << test_iter << ": send failed";

        /* Receive the item */
        std::vector<uint8_t> recv_data(item_size, 0);
        ASSERT_EQ(OSAL_OK,
                  osal_queue_receive(queue, recv_data.data(), OSAL_NO_WAIT))
            << "Iteration " << test_iter << ": receive failed";

        /* Verify round-trip consistency - received data must match sent data */
        EXPECT_EQ(0, std::memcmp(send_data.data(), recv_data.data(), item_size))
            << "Iteration " << test_iter << ": round-trip data mismatch "
            << "(item_size=" << item_size << ")";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/**
 * Feature: freertos-adapter, Property 9b: Queue Round-Trip with Multiple Items
 *
 * *For any* sequence of items sent to a queue, receiving them SHALL return
 * items with identical content in FIFO order.
 *
 * **Validates: Requirements 7.1, 7.3, 7.5**
 */
TEST_F(OsalQueuePropertyTest, Property9_QueueRoundTripMultipleItems) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Use fixed item size for this test */
        size_t item_size = sizeof(int);
        size_t capacity = randomCapacity();
        size_t num_items = randomItemCount(capacity);

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(item_size, capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Generate and send random values */
        std::vector<int> sent_values(num_items);
        for (size_t i = 0; i < num_items; i++) {
            sent_values[i] = randomValue();
            ASSERT_EQ(OSAL_OK,
                      osal_queue_send(queue, &sent_values[i], OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": send " << i << " failed";
        }

        /* Receive and verify each value */
        for (size_t i = 0; i < num_items; i++) {
            int recv_value = 0;
            ASSERT_EQ(OSAL_OK,
                      osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": receive " << i << " failed";

            EXPECT_EQ(sent_values[i], recv_value)
                << "Iteration " << test_iter
                << ": round-trip mismatch at index " << i << " (sent "
                << sent_values[i] << ", received " << recv_value << ")";
        }

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 10: Queue Count Accuracy                                         */
/* Feature: freertos-adapter, Property 10: Queue Count Accuracy              */
/*---------------------------------------------------------------------------*/

/**
 * Feature: freertos-adapter, Property 10: Queue Count Accuracy
 *
 * *For any* queue, after sending K items (K <= capacity) and receiving M items
 * (M <= K), osal_queue_get_count() SHALL return K-M.
 *
 * **Validates: Requirements 7.7**
 */
TEST_F(OsalQueuePropertyTest, Property10_QueueCountAccuracy) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        size_t capacity = randomCapacity();
        size_t num_sends = randomItemCount(capacity);

        /* Random number of receives (0 to num_sends) */
        std::uniform_int_distribution<size_t> recv_dist(0, num_sends);
        size_t num_receives = recv_dist(rng);

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Initial count should be 0 */
        EXPECT_EQ(0u, osal_queue_get_count(queue))
            << "Iteration " << test_iter << ": initial count should be 0";

        /* Send K items */
        for (size_t i = 0; i < num_sends; i++) {
            int value = static_cast<int>(i);
            ASSERT_EQ(OSAL_OK, osal_queue_send(queue, &value, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": send " << i << " failed";

            /* Verify count after each send */
            EXPECT_EQ(i + 1, osal_queue_get_count(queue))
                << "Iteration " << test_iter << ": count after send " << i
                << " incorrect";
        }

        /* Receive M items */
        for (size_t i = 0; i < num_receives; i++) {
            int value = 0;
            ASSERT_EQ(OSAL_OK, osal_queue_receive(queue, &value, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": receive " << i << " failed";

            /* Verify count after each receive */
            size_t expected_count = num_sends - (i + 1);
            EXPECT_EQ(expected_count, osal_queue_get_count(queue))
                << "Iteration " << test_iter << ": count after receive " << i
                << " incorrect "
                << "(expected " << expected_count << ")";
        }

        /* Final count should be K - M */
        size_t expected_final = num_sends - num_receives;
        EXPECT_EQ(expected_final, osal_queue_get_count(queue))
            << "Iteration " << test_iter << ": final count incorrect "
            << "(K=" << num_sends << ", M=" << num_receives
            << ", expected K-M=" << expected_final << ")";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 11: Queue Peek Does Not Remove                                   */
/* Feature: freertos-adapter, Property 11: Queue Peek Does Not Remove        */
/*---------------------------------------------------------------------------*/

/**
 * Feature: freertos-adapter, Property 11: Queue Peek Does Not Remove
 *
 * *For any* non-empty queue, calling osal_queue_peek() SHALL return the front
 * item without changing the queue count.
 *
 * **Validates: Requirements 7.6**
 */
TEST_F(OsalQueuePropertyTest, Property11_QueuePeekDoesNotRemove) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        size_t capacity = randomCapacity();
        size_t num_items = randomItemCount(capacity);

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Send items */
        std::vector<int> sent_values(num_items);
        for (size_t i = 0; i < num_items; i++) {
            sent_values[i] = randomValue();
            ASSERT_EQ(OSAL_OK,
                      osal_queue_send(queue, &sent_values[i], OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": send " << i << " failed";
        }

        /* Record count before peek */
        size_t count_before = osal_queue_get_count(queue);
        EXPECT_EQ(num_items, count_before)
            << "Iteration " << test_iter << ": count before peek incorrect";

        /* Peek multiple times - count should not change */
        for (int peek_iter = 0; peek_iter < 5; peek_iter++) {
            int peek_value = 0;
            ASSERT_EQ(OSAL_OK, osal_queue_peek(queue, &peek_value))
                << "Iteration " << test_iter << ": peek " << peek_iter
                << " failed";

            /* Peek should return the front item (first sent) */
            EXPECT_EQ(sent_values[0], peek_value)
                << "Iteration " << test_iter << ": peek " << peek_iter
                << " returned wrong value";

            /* Count should remain unchanged */
            EXPECT_EQ(count_before, osal_queue_get_count(queue))
                << "Iteration " << test_iter << ": peek " << peek_iter
                << " changed the count";
        }

        /* Verify receive still gets the same front item */
        int recv_value = 0;
        ASSERT_EQ(OSAL_OK, osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT))
            << "Iteration " << test_iter << ": receive after peek failed";

        EXPECT_EQ(sent_values[0], recv_value)
            << "Iteration " << test_iter
            << ": receive after peek got wrong value";

        /* Now count should be decremented */
        EXPECT_EQ(count_before - 1, osal_queue_get_count(queue))
            << "Iteration " << test_iter << ": count after receive incorrect";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 16: Queue FIFO Order                                             */
/*---------------------------------------------------------------------------*/

/**
 * Feature: phase2-core-platform, Property 16: Queue FIFO Order
 *
 * *For any* sequence of items sent to a queue, receiving them SHALL return
 * items in the same order (FIFO).
 *
 * **Validates: Requirements 10.2, 10.4**
 */
TEST_F(OsalQueuePropertyTest, Property16_QueueFIFOOrder) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        size_t capacity = randomCapacity();
        size_t num_items = randomItemCount(capacity);

        /* Generate random values to send */
        std::vector<int> send_values(num_items);
        for (size_t i = 0; i < num_items; i++) {
            send_values[i] = randomValue();
        }

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Send all items */
        for (size_t i = 0; i < num_items; i++) {
            ASSERT_EQ(OSAL_OK,
                      osal_queue_send(queue, &send_values[i], OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": send " << i << " failed";
        }

        /* Receive all items and verify FIFO order */
        for (size_t i = 0; i < num_items; i++) {
            int recv_value = 0;
            ASSERT_EQ(OSAL_OK,
                      osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": receive " << i << " failed";

            EXPECT_EQ(send_values[i], recv_value)
                << "Iteration " << test_iter
                << ": FIFO order violated at index " << i << " (expected "
                << send_values[i] << ", got " << recv_value << ")";
        }

        /* Queue should be empty */
        EXPECT_TRUE(osal_queue_is_empty(queue))
            << "Iteration " << test_iter
            << ": queue should be empty after receiving all items";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/**
 * Feature: phase2-core-platform, Property 16b: Queue FIFO Order with
 * Interleaved Operations
 *
 * *For any* sequence of interleaved send/receive operations, the relative order
 * of items SHALL be preserved (FIFO).
 *
 * **Validates: Requirements 10.2, 10.4**
 */
TEST_F(OsalQueuePropertyTest, Property16_QueueFIFOOrderInterleaved) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        size_t capacity = randomCapacity();

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        std::vector<int> expected_order;
        int send_counter = 0;

        /* Perform random interleaved operations */
        for (int op = 0; op < 50; op++) {
            bool do_send = (rng() % 2 == 0) || osal_queue_is_empty(queue);
            bool queue_full = osal_queue_is_full(queue);

            if (do_send && !queue_full) {
                /* Send a new item */
                int value = send_counter++;
                ASSERT_EQ(OSAL_OK, osal_queue_send(queue, &value, OSAL_NO_WAIT))
                    << "Iteration " << test_iter << ": send failed at op "
                    << op;
                expected_order.push_back(value);
            } else if (!osal_queue_is_empty(queue)) {
                /* Receive an item */
                int recv_value = 0;
                ASSERT_EQ(OSAL_OK,
                          osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT))
                    << "Iteration " << test_iter << ": receive failed at op "
                    << op;

                /* Verify FIFO order */
                ASSERT_FALSE(expected_order.empty())
                    << "Iteration " << test_iter
                    << ": expected_order is empty but received value";

                EXPECT_EQ(expected_order.front(), recv_value)
                    << "Iteration " << test_iter
                    << ": FIFO order violated at op " << op;

                expected_order.erase(expected_order.begin());
            }
        }

        /* Drain remaining items and verify order */
        while (!osal_queue_is_empty(queue)) {
            int recv_value = 0;
            ASSERT_EQ(OSAL_OK,
                      osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT));

            ASSERT_FALSE(expected_order.empty());
            EXPECT_EQ(expected_order.front(), recv_value)
                << "Iteration " << test_iter
                << ": FIFO order violated during drain";
            expected_order.erase(expected_order.begin());
        }

        EXPECT_TRUE(expected_order.empty())
            << "Iteration " << test_iter << ": expected_order should be empty";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 17: Queue Capacity                                               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: phase2-core-platform, Property 17: Queue Capacity
 *
 * *For any* queue with capacity N, sending N+1 items without receiving
 * SHALL block on the (N+1)th send.
 *
 * **Validates: Requirements 10.1, 10.3**
 */
TEST_F(OsalQueuePropertyTest, Property17_QueueCapacity) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random capacity */
        size_t capacity = randomCapacity();

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Fill the queue to capacity */
        for (size_t i = 0; i < capacity; i++) {
            int value = static_cast<int>(i);
            ASSERT_EQ(OSAL_OK, osal_queue_send(queue, &value, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": send " << i
                << " should succeed (capacity=" << capacity << ")";
        }

        /* Queue should be full */
        EXPECT_TRUE(osal_queue_is_full(queue))
            << "Iteration " << test_iter << ": queue should be full after "
            << capacity << " sends";

        EXPECT_EQ(capacity, osal_queue_get_count(queue))
            << "Iteration " << test_iter
            << ": queue count should equal capacity";

        /* The (capacity + 1)th send should fail/block */
        int extra_value = 999;
        EXPECT_EQ(OSAL_ERROR_FULL,
                  osal_queue_send(queue, &extra_value, OSAL_NO_WAIT))
            << "Iteration " << test_iter
            << ": send after reaching capacity should return FULL";

        /* Receive one item */
        int recv_value = 0;
        ASSERT_EQ(OSAL_OK, osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT))
            << "Iteration " << test_iter << ": receive should succeed";

        /* Now send should succeed again */
        EXPECT_EQ(OSAL_OK, osal_queue_send(queue, &extra_value, OSAL_NO_WAIT))
            << "Iteration " << test_iter
            << ": send after receiving should succeed";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/**
 * Feature: phase2-core-platform, Property 17b: Queue Capacity with Different
 * Item Sizes
 *
 * *For any* queue with capacity N and item size S, the queue SHALL hold
 * exactly N items regardless of item size.
 *
 * **Validates: Requirements 10.1**
 */
TEST_F(OsalQueuePropertyTest, Property17_QueueCapacityDifferentSizes) {
    /* Test with different item sizes */
    std::vector<size_t> item_sizes = {1, 4, 16, 64, 128};

    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Pick random item size */
        size_t item_size = item_sizes[rng() % item_sizes.size()];
        size_t capacity = randomCapacity();

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(item_size, capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed "
            << "(item_size=" << item_size << ", capacity=" << capacity << ")";

        /* Allocate buffer for items */
        std::vector<uint8_t> item(item_size);

        /* Fill the queue to capacity */
        for (size_t i = 0; i < capacity; i++) {
            /* Fill item with pattern */
            std::fill(item.begin(), item.end(), static_cast<uint8_t>(i & 0xFF));

            ASSERT_EQ(OSAL_OK,
                      osal_queue_send(queue, item.data(), OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": send " << i
                << " should succeed";
        }

        /* Verify capacity */
        EXPECT_EQ(capacity, osal_queue_get_count(queue))
            << "Iteration " << test_iter
            << ": queue count should equal capacity";

        EXPECT_TRUE(osal_queue_is_full(queue))
            << "Iteration " << test_iter << ": queue should be full";

        /* Extra send should fail */
        EXPECT_EQ(OSAL_ERROR_FULL,
                  osal_queue_send(queue, item.data(), OSAL_NO_WAIT))
            << "Iteration " << test_iter << ": send when full should fail";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/**
 * Feature: phase2-core-platform, Property 17c: Queue Empty After Draining
 *
 * *For any* queue with N items, receiving N items SHALL result in an empty
 * queue.
 *
 * **Validates: Requirements 10.4**
 */
TEST_F(OsalQueuePropertyTest, Property17_QueueEmptyAfterDraining) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random parameters */
        size_t capacity = randomCapacity();
        size_t num_items = randomItemCount(capacity);

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Send items */
        for (size_t i = 0; i < num_items; i++) {
            int value = static_cast<int>(i);
            ASSERT_EQ(OSAL_OK, osal_queue_send(queue, &value, OSAL_NO_WAIT));
        }

        EXPECT_EQ(num_items, osal_queue_get_count(queue));

        /* Receive all items */
        for (size_t i = 0; i < num_items; i++) {
            int recv_value = 0;
            ASSERT_EQ(OSAL_OK,
                      osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT));
        }

        /* Queue should be empty */
        EXPECT_TRUE(osal_queue_is_empty(queue))
            << "Iteration " << test_iter
            << ": queue should be empty after draining";

        EXPECT_EQ(0u, osal_queue_get_count(queue))
            << "Iteration " << test_iter << ": queue count should be 0";

        /* Receive on empty queue should fail */
        int value = 0;
        EXPECT_EQ(OSAL_ERROR_EMPTY,
                  osal_queue_receive(queue, &value, OSAL_NO_WAIT))
            << "Iteration " << test_iter
            << ": receive on empty queue should fail";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 12: Queue Space Invariant                                        */
/* Feature: osal-refactor, Property 12: Queue Space Invariant                */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property 12: Queue Space Invariant
 *
 * *For any* queue with capacity C, osal_queue_get_available_space() +
 * osal_queue_get_count() SHALL equal C.
 *
 * **Validates: Requirements 8.1**
 */
TEST_F(OsalQueuePropertyTest, Property12_QueueSpaceInvariant) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random capacity */
        size_t capacity = randomCapacity();

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Initial state: empty queue */
        size_t count = osal_queue_get_count(queue);
        size_t available = osal_queue_get_available_space(queue);
        EXPECT_EQ(capacity, count + available)
            << "Iteration " << test_iter
            << ": invariant violated at initial state "
            << "(count=" << count << ", available=" << available
            << ", capacity=" << capacity << ")";

        /* Random number of items to send */
        size_t num_items = randomItemCount(capacity);

        /* Send items and verify invariant after each send */
        for (size_t i = 0; i < num_items; i++) {
            int value = static_cast<int>(i);
            ASSERT_EQ(OSAL_OK, osal_queue_send(queue, &value, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": send " << i << " failed";

            count = osal_queue_get_count(queue);
            available = osal_queue_get_available_space(queue);
            EXPECT_EQ(capacity, count + available)
                << "Iteration " << test_iter
                << ": invariant violated after send " << i
                << " (count=" << count << ", available=" << available
                << ", capacity=" << capacity << ")";
        }

        /* Receive some items and verify invariant */
        size_t num_receives = num_items / 2;
        for (size_t i = 0; i < num_receives; i++) {
            int recv_value = 0;
            ASSERT_EQ(OSAL_OK,
                      osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": receive " << i << " failed";

            count = osal_queue_get_count(queue);
            available = osal_queue_get_available_space(queue);
            EXPECT_EQ(capacity, count + available)
                << "Iteration " << test_iter
                << ": invariant violated after receive " << i
                << " (count=" << count << ", available=" << available
                << ", capacity=" << capacity << ")";
        }

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 13: Queue Reset Clears All                                       */
/* Feature: osal-refactor, Property 13: Queue Reset Clears All               */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property 13: Queue Reset Clears All
 *
 * *For any* queue with items, after calling osal_queue_reset(),
 * osal_queue_get_count() SHALL return 0 and osal_queue_is_empty() SHALL
 * return true.
 *
 * **Validates: Requirements 8.2**
 */
TEST_F(OsalQueuePropertyTest, Property13_QueueResetClearsAll) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random capacity */
        size_t capacity = randomCapacity();

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Random number of items to send */
        size_t num_items = randomItemCount(capacity);

        /* Send items */
        for (size_t i = 0; i < num_items; i++) {
            int value = static_cast<int>(i);
            ASSERT_EQ(OSAL_OK, osal_queue_send(queue, &value, OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": send " << i << " failed";
        }

        /* Verify queue has items */
        EXPECT_EQ(num_items, osal_queue_get_count(queue))
            << "Iteration " << test_iter << ": queue should have items";
        EXPECT_FALSE(osal_queue_is_empty(queue))
            << "Iteration " << test_iter << ": queue should not be empty";

        /* Reset the queue */
        ASSERT_EQ(OSAL_OK, osal_queue_reset(queue))
            << "Iteration " << test_iter << ": queue reset failed";

        /* Verify queue is empty after reset */
        EXPECT_EQ(0u, osal_queue_get_count(queue))
            << "Iteration " << test_iter
            << ": queue count should be 0 after reset";
        EXPECT_TRUE(osal_queue_is_empty(queue))
            << "Iteration " << test_iter
            << ": queue should be empty after reset";

        /* Verify available space equals capacity after reset */
        EXPECT_EQ(capacity, osal_queue_get_available_space(queue))
            << "Iteration " << test_iter
            << ": available space should equal capacity after reset";

        /* Verify receive fails on empty queue */
        int recv_value = 0;
        EXPECT_EQ(OSAL_ERROR_EMPTY,
                  osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT))
            << "Iteration " << test_iter
            << ": receive should fail on reset queue";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 14: Queue Overwrite Mode Behavior                                */
/* Feature: osal-refactor, Property 14: Queue Overwrite Mode Behavior        */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property 14: Queue Overwrite Mode Behavior
 *
 * *For any* queue in overwrite mode, osal_queue_send() SHALL always succeed
 * (return OSAL_OK) regardless of queue fullness.
 *
 * **Validates: Requirements 8.3, 8.4**
 *
 * Note: This test verifies the API accepts the mode setting. Full overwrite
 * behavior depends on platform-specific implementation.
 */
TEST_F(OsalQueuePropertyTest, Property14_QueueOverwriteModeBehavior) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random capacity */
        size_t capacity = randomCapacity();

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Set overwrite mode - should succeed */
        EXPECT_EQ(OSAL_OK,
                  osal_queue_set_mode(queue, OSAL_QUEUE_MODE_OVERWRITE))
            << "Iteration " << test_iter << ": set overwrite mode failed";

        /* Set normal mode - should succeed */
        EXPECT_EQ(OSAL_OK, osal_queue_set_mode(queue, OSAL_QUEUE_MODE_NORMAL))
            << "Iteration " << test_iter << ": set normal mode failed";

        /* Test invalid mode */
        EXPECT_EQ(OSAL_ERROR_INVALID_PARAM,
                  osal_queue_set_mode(queue, (osal_queue_mode_t)99))
            << "Iteration " << test_iter
            << ": invalid mode should return error";

        /* Test NULL handle */
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
                  osal_queue_set_mode(nullptr, OSAL_QUEUE_MODE_NORMAL))
            << "Iteration " << test_iter << ": NULL handle should return error";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}

/*---------------------------------------------------------------------------*/
/* Property 15: Queue Peek From ISR                                          */
/* Feature: osal-refactor, Property 15: Queue Peek From ISR                  */
/*---------------------------------------------------------------------------*/

/**
 * Feature: osal-refactor, Property 15: Queue Peek From ISR
 *
 * *For any* non-empty queue, osal_queue_peek_from_isr() SHALL return the
 * front item without removing it, and subsequent peek SHALL return the
 * same item.
 *
 * **Validates: Requirements 8.5**
 */
TEST_F(OsalQueuePropertyTest, Property15_QueuePeekFromISR) {
    for (int test_iter = 0; test_iter < PROPERTY_TEST_ITERATIONS; ++test_iter) {
        /* Generate random capacity */
        size_t capacity = randomCapacity();
        size_t num_items = randomItemCount(capacity);

        /* Create queue */
        osal_queue_handle_t queue = nullptr;
        ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(int), capacity, &queue))
            << "Iteration " << test_iter << ": queue create failed";

        /* Send items */
        std::vector<int> sent_values(num_items);
        for (size_t i = 0; i < num_items; i++) {
            sent_values[i] = randomValue();
            ASSERT_EQ(OSAL_OK,
                      osal_queue_send(queue, &sent_values[i], OSAL_NO_WAIT))
                << "Iteration " << test_iter << ": send " << i << " failed";
        }

        /* Record count before peek */
        size_t count_before = osal_queue_get_count(queue);

        /* Peek from ISR multiple times - should return same value */
        for (int peek_iter = 0; peek_iter < 5; peek_iter++) {
            int peek_value = 0;
            ASSERT_EQ(OSAL_OK, osal_queue_peek_from_isr(queue, &peek_value))
                << "Iteration " << test_iter << ": peek_from_isr " << peek_iter
                << " failed";

            /* Peek should return the front item (first sent) */
            EXPECT_EQ(sent_values[0], peek_value)
                << "Iteration " << test_iter << ": peek_from_isr " << peek_iter
                << " returned wrong value";

            /* Count should remain unchanged */
            EXPECT_EQ(count_before, osal_queue_get_count(queue))
                << "Iteration " << test_iter << ": peek_from_isr " << peek_iter
                << " changed the count";
        }

        /* Verify receive still gets the same front item */
        int recv_value = 0;
        ASSERT_EQ(OSAL_OK, osal_queue_receive(queue, &recv_value, OSAL_NO_WAIT))
            << "Iteration " << test_iter << ": receive after peek failed";

        EXPECT_EQ(sent_values[0], recv_value)
            << "Iteration " << test_iter
            << ": receive after peek got wrong value";

        /* Test peek on empty queue */
        osal_queue_reset(queue);
        int empty_peek = 0;
        EXPECT_EQ(OSAL_ERROR_EMPTY,
                  osal_queue_peek_from_isr(queue, &empty_peek))
            << "Iteration " << test_iter
            << ": peek_from_isr on empty queue should fail";

        /* Test NULL handle */
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
                  osal_queue_peek_from_isr(nullptr, &empty_peek))
            << "Iteration " << test_iter
            << ": peek_from_isr with NULL handle should fail";

        /* Test NULL item pointer */
        EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
                  osal_queue_peek_from_isr(queue, nullptr))
            << "Iteration " << test_iter
            << ": peek_from_isr with NULL item should fail";

        /* Clean up */
        ASSERT_EQ(OSAL_OK, osal_queue_delete(queue))
            << "Iteration " << test_iter << ": queue delete failed";
    }
}
