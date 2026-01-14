/**
 * \file            test_osal_queue.cpp
 * \brief           OSAL Queue Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Unit tests for OSAL Queue module.
 *                  Requirements: 10.1, 10.2, 10.4, 10.7
 */

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "osal/osal.h"
}

/**
 * \brief           OSAL Queue Test Fixture
 */
class OsalQueueTest : public ::testing::Test {
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
/* Queue Creation Tests - Requirements 10.1                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test queue creation with valid parameters
 * \details         Requirements 10.1 - Queue creation should succeed
 */
TEST_F(OsalQueueTest, CreateQueue) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 10, &handle));
    EXPECT_NE(nullptr, handle);

    /* Clean up */
    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue creation with different item sizes
 */
TEST_F(OsalQueueTest, CreateQueueDifferentSizes) {
    osal_queue_handle_t handle = nullptr;

    /* Small item */
    EXPECT_EQ(OSAL_OK, osal_queue_create(1, 10, &handle));
    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));

    /* Medium item */
    EXPECT_EQ(OSAL_OK, osal_queue_create(64, 10, &handle));
    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));

    /* Large item */
    EXPECT_EQ(OSAL_OK, osal_queue_create(256, 5, &handle));
    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue creation with null handle
 */
TEST_F(OsalQueueTest, CreateWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
              osal_queue_create(sizeof(int), 10, nullptr));
}

/**
 * \brief           Test queue creation with invalid parameters
 */
TEST_F(OsalQueueTest, CreateWithInvalidParams) {
    osal_queue_handle_t handle = nullptr;

    /* Zero item size */
    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM, osal_queue_create(0, 10, &handle));

    /* Zero item count */
    EXPECT_EQ(OSAL_ERROR_INVALID_PARAM,
              osal_queue_create(sizeof(int), 0, &handle));
}

/**
 * \brief           Test creating multiple queues
 */
TEST_F(OsalQueueTest, CreateMultipleQueues) {
    const int num_queues = 4;
    osal_queue_handle_t handles[num_queues];

    for (int i = 0; i < num_queues; i++) {
        EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 10, &handles[i]));
        EXPECT_NE(nullptr, handles[i]);
    }

    /* Clean up */
    for (int i = 0; i < num_queues; i++) {
        EXPECT_EQ(OSAL_OK, osal_queue_delete(handles[i]));
    }
}

/*---------------------------------------------------------------------------*/
/* Queue Delete Tests - Requirements 10.7                                    */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test queue deletion
 * \details         Requirements 10.7 - Queue deletion should succeed
 */
TEST_F(OsalQueueTest, DeleteQueue) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 10, &handle));
    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue deletion with null handle
 */
TEST_F(OsalQueueTest, DeleteWithNullHandle) {
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER, osal_queue_delete(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Queue Send Tests - Requirements 10.2                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test queue send when not full
 * \details         Requirements 10.2 - Send should succeed when queue is not
 * full
 */
TEST_F(OsalQueueTest, SendWhenNotFull) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    int value = 42;
    EXPECT_EQ(OSAL_OK, osal_queue_send(handle, &value, OSAL_NO_WAIT));

    /* Verify queue is not empty */
    EXPECT_FALSE(osal_queue_is_empty(handle));
    EXPECT_EQ(1u, osal_queue_get_count(handle));

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue send multiple items
 */
TEST_F(OsalQueueTest, SendMultipleItems) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(OSAL_OK, osal_queue_send(handle, &i, OSAL_NO_WAIT));
    }

    EXPECT_TRUE(osal_queue_is_full(handle));
    EXPECT_EQ(5u, osal_queue_get_count(handle));

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue send when full (no wait)
 */
TEST_F(OsalQueueTest, SendWhenFullNoWait) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 3, &handle));

    /* Fill the queue */
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(OSAL_OK, osal_queue_send(handle, &i, OSAL_NO_WAIT));
    }

    /* Next send should fail */
    int value = 99;
    EXPECT_EQ(OSAL_ERROR_FULL, osal_queue_send(handle, &value, OSAL_NO_WAIT));

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue send with null handle
 */
TEST_F(OsalQueueTest, SendWithNullHandle) {
    int value = 42;
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
              osal_queue_send(nullptr, &value, OSAL_NO_WAIT));
}

/**
 * \brief           Test queue send with null item
 */
TEST_F(OsalQueueTest, SendWithNullItem) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
              osal_queue_send(handle, nullptr, OSAL_NO_WAIT));

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Queue Receive Tests - Requirements 10.4                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test queue receive when not empty
 * \details         Requirements 10.4 - Receive should succeed when queue is not
 * empty
 */
TEST_F(OsalQueueTest, ReceiveWhenNotEmpty) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    int send_value = 42;
    EXPECT_EQ(OSAL_OK, osal_queue_send(handle, &send_value, OSAL_NO_WAIT));

    int recv_value = 0;
    EXPECT_EQ(OSAL_OK, osal_queue_receive(handle, &recv_value, OSAL_NO_WAIT));
    EXPECT_EQ(send_value, recv_value);

    /* Queue should be empty now */
    EXPECT_TRUE(osal_queue_is_empty(handle));

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue receive when empty (no wait)
 */
TEST_F(OsalQueueTest, ReceiveWhenEmptyNoWait) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    int value = 0;
    EXPECT_EQ(OSAL_ERROR_EMPTY,
              osal_queue_receive(handle, &value, OSAL_NO_WAIT));

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue receive with timeout when empty
 */
TEST_F(OsalQueueTest, ReceiveTimeoutWhenEmpty) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    int value = 0;
    auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(OSAL_ERROR_TIMEOUT, osal_queue_receive(handle, &value, 100));
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    /* Should have waited approximately 100ms */
    EXPECT_GE(elapsed_ms, 80);

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue receive with null handle
 */
TEST_F(OsalQueueTest, ReceiveWithNullHandle) {
    int value = 0;
    EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
              osal_queue_receive(nullptr, &value, OSAL_NO_WAIT));
}

/**
 * \brief           Test queue receive with null item
 */
TEST_F(OsalQueueTest, ReceiveWithNullItem) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    int value = 42;
    EXPECT_EQ(OSAL_OK, osal_queue_send(handle, &value, OSAL_NO_WAIT));

    EXPECT_EQ(OSAL_ERROR_NULL_POINTER,
              osal_queue_receive(handle, nullptr, OSAL_NO_WAIT));

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Queue Send/Receive Sequence Tests                                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test multiple send/receive cycles
 */
TEST_F(OsalQueueTest, MultipleSendReceiveCycles) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 10, &handle));

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(OSAL_OK, osal_queue_send(handle, &i, OSAL_NO_WAIT));

        int recv_value = 0;
        EXPECT_EQ(OSAL_OK,
                  osal_queue_receive(handle, &recv_value, OSAL_NO_WAIT));
        EXPECT_EQ(i, recv_value);
    }

    EXPECT_TRUE(osal_queue_is_empty(handle));

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue with struct items
 */
TEST_F(OsalQueueTest, QueueWithStructItems) {
    struct TestItem {
        int id;
        char data[16];
        float value;
    };

    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(TestItem), 5, &handle));

    TestItem send_item = {42, "test", 3.14f};
    EXPECT_EQ(OSAL_OK, osal_queue_send(handle, &send_item, OSAL_NO_WAIT));

    TestItem recv_item = {};
    EXPECT_EQ(OSAL_OK, osal_queue_receive(handle, &recv_item, OSAL_NO_WAIT));

    EXPECT_EQ(send_item.id, recv_item.id);
    EXPECT_STREQ(send_item.data, recv_item.data);
    EXPECT_FLOAT_EQ(send_item.value, recv_item.value);

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Queue Peek Tests                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test queue peek
 */
TEST_F(OsalQueueTest, PeekQueue) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    int value = 42;
    EXPECT_EQ(OSAL_OK, osal_queue_send(handle, &value, OSAL_NO_WAIT));

    /* Peek should return the value without removing it */
    int peek_value = 0;
    EXPECT_EQ(OSAL_OK, osal_queue_peek(handle, &peek_value));
    EXPECT_EQ(value, peek_value);

    /* Queue should still have the item */
    EXPECT_EQ(1u, osal_queue_get_count(handle));

    /* Receive should get the same value */
    int recv_value = 0;
    EXPECT_EQ(OSAL_OK, osal_queue_receive(handle, &recv_value, OSAL_NO_WAIT));
    EXPECT_EQ(value, recv_value);

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue peek when empty
 */
TEST_F(OsalQueueTest, PeekWhenEmpty) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    int value = 0;
    EXPECT_EQ(OSAL_ERROR_EMPTY, osal_queue_peek(handle, &value));

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Queue ISR Functions Tests                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Test queue send from ISR
 */
TEST_F(OsalQueueTest, SendFromIsr) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    int value = 42;
    EXPECT_EQ(OSAL_OK, osal_queue_send_from_isr(handle, &value));

    int recv_value = 0;
    EXPECT_EQ(OSAL_OK, osal_queue_receive(handle, &recv_value, OSAL_NO_WAIT));
    EXPECT_EQ(value, recv_value);

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/**
 * \brief           Test queue receive from ISR
 */
TEST_F(OsalQueueTest, ReceiveFromIsr) {
    osal_queue_handle_t handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 5, &handle));

    int value = 42;
    EXPECT_EQ(OSAL_OK, osal_queue_send(handle, &value, OSAL_NO_WAIT));

    int recv_value = 0;
    EXPECT_EQ(OSAL_OK, osal_queue_receive_from_isr(handle, &recv_value));
    EXPECT_EQ(value, recv_value);

    EXPECT_EQ(OSAL_OK, osal_queue_delete(handle));
}

/*---------------------------------------------------------------------------*/
/* Queue Multi-Task Tests                                                    */
/*---------------------------------------------------------------------------*/

static std::atomic<bool> s_queue_producer_done{false};
static std::atomic<int> s_queue_received_count{0};
static osal_queue_handle_t s_shared_queue = nullptr;

static void queue_producer_task(void* arg) {
    int count = *static_cast<int*>(arg);

    for (int i = 0; i < count; i++) {
        osal_queue_send(s_shared_queue, &i, OSAL_WAIT_FOREVER);
        osal_task_delay(5);
    }

    s_queue_producer_done = true;
}

static void queue_consumer_task(void* arg) {
    (void)arg;
    int value = 0;

    while (!s_queue_producer_done || !osal_queue_is_empty(s_shared_queue)) {
        if (osal_queue_receive(s_shared_queue, &value, 50) == OSAL_OK) {
            s_queue_received_count++;
        }
    }
}

/**
 * \brief           Test queue with producer/consumer pattern
 */
TEST_F(OsalQueueTest, ProducerConsumerPattern) {
    s_queue_producer_done = false;
    s_queue_received_count = 0;

    EXPECT_EQ(OSAL_OK, osal_queue_create(sizeof(int), 20, &s_shared_queue));

    int produce_count = 10;

    /* Create producer task */
    osal_task_config_t producer_config = {.name = "producer",
                                          .func = queue_producer_task,
                                          .arg = &produce_count,
                                          .priority = OSAL_TASK_PRIORITY_NORMAL,
                                          .stack_size = 4096};

    osal_task_handle_t producer_handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&producer_config, &producer_handle));

    /* Create consumer task */
    osal_task_config_t consumer_config = {.name = "consumer",
                                          .func = queue_consumer_task,
                                          .arg = nullptr,
                                          .priority = OSAL_TASK_PRIORITY_NORMAL,
                                          .stack_size = 4096};

    osal_task_handle_t consumer_handle = nullptr;
    EXPECT_EQ(OSAL_OK, osal_task_create(&consumer_config, &consumer_handle));

    /* Wait for producer to finish */
    auto start = std::chrono::steady_clock::now();
    while (!s_queue_producer_done) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(5)) {
            FAIL() << "Producer did not finish in time";
        }
    }

    /* Wait a bit for consumer to finish */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* Verify all items were received */
    EXPECT_EQ(produce_count, s_queue_received_count);

    /* Clean up */
    osal_task_delete(producer_handle);
    osal_task_delete(consumer_handle);
    osal_queue_delete(s_shared_queue);
    s_shared_queue = nullptr;
}
