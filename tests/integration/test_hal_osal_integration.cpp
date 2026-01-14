/**
 * \file            test_hal_osal_integration.cpp
 * \brief           HAL + OSAL Integration Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-13
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Integration tests for HAL and OSAL modules working together.
 *                  Tests multi-task HAL usage and inter-task communication.
 *                  Requirements: 13.2, 13.3
 */

#include <atomic>
#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>

extern "C" {
#include "hal/hal.h"
#include "osal/osal.h"
}

/**
 * \brief           HAL + OSAL Integration Test Fixture
 */
class HalOsalIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        hal_init();
        osal_init();
    }

    void TearDown() override {
        /* Allow tasks to clean up */
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        hal_deinit();
    }
};

/*---------------------------------------------------------------------------*/
/* Test Data Structures                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Message structure for queue tests
 */
typedef struct {
    uint32_t id;
    uint32_t data;
    uint32_t timestamp;
} test_message_t;

/**
 * \brief           Shared state for multi-task GPIO tests
 */
static struct {
    std::atomic<int> task1_toggles{0};
    std::atomic<int> task2_toggles{0};
    std::atomic<bool> running{false};
    osal_mutex_handle_t gpio_mutex;
} s_gpio_test_state;

/**
 * \brief           Shared state for producer-consumer tests
 */
static struct {
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> running{false};
    osal_queue_handle_t queue;
    osal_sem_handle_t done_sem;
} s_queue_test_state;

/**
 * \brief           Shared state for UART multi-task tests
 */
static struct {
    std::atomic<int> tx_count{0};
    std::atomic<int> rx_count{0};
    std::atomic<bool> running{false};
    osal_mutex_handle_t uart_mutex;
    osal_sem_handle_t sync_sem;
} s_uart_test_state;

/*---------------------------------------------------------------------------*/
/* Multi-Task GPIO Tests - Requirements 13.2                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Task 1: Toggle GPIO pin with mutex protection
 */
static void gpio_task1_func(void* arg) {
    (void)arg;
    hal_gpio_port_t port = HAL_GPIO_PORT_A;
    hal_gpio_pin_t pin = 0;

    while (s_gpio_test_state.running) {
        osal_mutex_lock(s_gpio_test_state.gpio_mutex, OSAL_WAIT_FOREVER);
        hal_gpio_toggle(port, pin);
        s_gpio_test_state.task1_toggles++;
        osal_mutex_unlock(s_gpio_test_state.gpio_mutex);
        osal_task_delay(10);
    }
}

/**
 * \brief           Task 2: Toggle GPIO pin with mutex protection
 */
static void gpio_task2_func(void* arg) {
    (void)arg;
    hal_gpio_port_t port = HAL_GPIO_PORT_A;
    hal_gpio_pin_t pin = 1;

    while (s_gpio_test_state.running) {
        osal_mutex_lock(s_gpio_test_state.gpio_mutex, OSAL_WAIT_FOREVER);
        hal_gpio_toggle(port, pin);
        s_gpio_test_state.task2_toggles++;
        osal_mutex_unlock(s_gpio_test_state.gpio_mutex);
        osal_task_delay(10);
    }
}

/**
 * \brief           Test multiple tasks accessing GPIO with mutex protection
 * \details         Requirements 13.2 - Multi-task HAL usage
 */
TEST_F(HalOsalIntegrationTest, MultiTaskGpioWithMutex) {
    /* Initialize GPIO pins */
    hal_gpio_config_t config = {.direction = HAL_GPIO_DIR_OUTPUT,
                                .pull = HAL_GPIO_PULL_NONE,
                                .output_mode = HAL_GPIO_OUTPUT_PP,
                                .speed = HAL_GPIO_SPEED_LOW,
                                .init_level = HAL_GPIO_LEVEL_LOW};

    ASSERT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 0, &config));
    ASSERT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 1, &config));

    /* Create mutex */
    ASSERT_EQ(OSAL_OK, osal_mutex_create(&s_gpio_test_state.gpio_mutex));

    /* Reset state */
    s_gpio_test_state.task1_toggles = 0;
    s_gpio_test_state.task2_toggles = 0;
    s_gpio_test_state.running = true;

    /* Create tasks */
    osal_task_config_t task1_config = {.name = "gpio_task1",
                                       .func = gpio_task1_func,
                                       .arg = nullptr,
                                       .priority = OSAL_TASK_PRIORITY_NORMAL,
                                       .stack_size = 4096};

    osal_task_config_t task2_config = {.name = "gpio_task2",
                                       .func = gpio_task2_func,
                                       .arg = nullptr,
                                       .priority = OSAL_TASK_PRIORITY_NORMAL,
                                       .stack_size = 4096};

    osal_task_handle_t task1 = nullptr;
    osal_task_handle_t task2 = nullptr;

    ASSERT_EQ(OSAL_OK, osal_task_create(&task1_config, &task1));
    ASSERT_EQ(OSAL_OK, osal_task_create(&task2_config, &task2));

    /* Let tasks run for a while */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* Stop tasks */
    s_gpio_test_state.running = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Verify both tasks ran */
    EXPECT_GT(s_gpio_test_state.task1_toggles.load(), 0);
    EXPECT_GT(s_gpio_test_state.task2_toggles.load(), 0);

    /* Clean up */
    osal_task_delete(task1);
    osal_task_delete(task2);
    osal_mutex_delete(s_gpio_test_state.gpio_mutex);
    hal_gpio_deinit(HAL_GPIO_PORT_A, 0);
    hal_gpio_deinit(HAL_GPIO_PORT_A, 1);
}

/*---------------------------------------------------------------------------*/
/* Producer-Consumer Queue Tests - Requirements 13.3                         */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Producer task: sends messages to queue
 */
static void producer_task_func(void* arg) {
    int num_messages = (arg != nullptr) ? *((int*)arg) : 10;

    for (int i = 0; i < num_messages && s_queue_test_state.running; i++) {
        test_message_t msg = {.id = (uint32_t)i,
                              .data = (uint32_t)(i * 100),
                              .timestamp = hal_get_tick()};

        if (osal_queue_send(s_queue_test_state.queue, &msg, 1000) == OSAL_OK) {
            s_queue_test_state.produced++;
        }
        osal_task_delay(5);
    }

    /* Signal completion */
    osal_sem_give(s_queue_test_state.done_sem);
}

/**
 * \brief           Consumer task: receives messages from queue
 */
static void consumer_task_func(void* arg) {
    int num_messages = (arg != nullptr) ? *((int*)arg) : 10;

    for (int i = 0; i < num_messages && s_queue_test_state.running; i++) {
        test_message_t msg;

        if (osal_queue_receive(s_queue_test_state.queue, &msg, 1000) ==
            OSAL_OK) {
            s_queue_test_state.consumed++;
            /* Verify message integrity */
            EXPECT_EQ(msg.data, msg.id * 100);
        }
    }

    /* Signal completion */
    osal_sem_give(s_queue_test_state.done_sem);
}

/**
 * \brief           Test producer-consumer pattern with queue
 * \details         Requirements 13.3 - Inter-task communication
 */
TEST_F(HalOsalIntegrationTest, ProducerConsumerQueue) {
    const int NUM_MESSAGES = 20;
    int msg_count = NUM_MESSAGES;

    /* Create queue and semaphore */
    ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(test_message_t), 10,
                                         &s_queue_test_state.queue));
    ASSERT_EQ(OSAL_OK, osal_sem_create(0, 2, &s_queue_test_state.done_sem));

    /* Reset state */
    s_queue_test_state.produced = 0;
    s_queue_test_state.consumed = 0;
    s_queue_test_state.running = true;

    /* Create tasks */
    osal_task_config_t producer_config = {.name = "producer",
                                          .func = producer_task_func,
                                          .arg = &msg_count,
                                          .priority = OSAL_TASK_PRIORITY_NORMAL,
                                          .stack_size = 4096};

    osal_task_config_t consumer_config = {.name = "consumer",
                                          .func = consumer_task_func,
                                          .arg = &msg_count,
                                          .priority = OSAL_TASK_PRIORITY_NORMAL,
                                          .stack_size = 4096};

    osal_task_handle_t producer = nullptr;
    osal_task_handle_t consumer = nullptr;

    ASSERT_EQ(OSAL_OK, osal_task_create(&producer_config, &producer));
    ASSERT_EQ(OSAL_OK, osal_task_create(&consumer_config, &consumer));

    /* Wait for both tasks to complete */
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_queue_test_state.done_sem, 5000));
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_queue_test_state.done_sem, 5000));

    /* Stop tasks */
    s_queue_test_state.running = false;

    /* Verify all messages were produced and consumed */
    EXPECT_EQ(NUM_MESSAGES, s_queue_test_state.produced.load());
    EXPECT_EQ(NUM_MESSAGES, s_queue_test_state.consumed.load());

    /* Clean up */
    osal_task_delete(producer);
    osal_task_delete(consumer);
    osal_queue_delete(s_queue_test_state.queue);
    osal_sem_delete(s_queue_test_state.done_sem);
}

/*---------------------------------------------------------------------------*/
/* Multi-Task UART Tests - Requirements 13.2                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART transmit task
 */
static void uart_tx_task_func(void* arg) {
    int num_bytes = (arg != nullptr) ? *((int*)arg) : 10;

    for (int i = 0; i < num_bytes && s_uart_test_state.running; i++) {
        osal_mutex_lock(s_uart_test_state.uart_mutex, OSAL_WAIT_FOREVER);

        uint8_t byte = (uint8_t)(i & 0xFF);
        if (hal_uart_putc(HAL_UART_0, byte) == HAL_OK) {
            s_uart_test_state.tx_count++;
        }

        osal_mutex_unlock(s_uart_test_state.uart_mutex);
        osal_task_delay(5);
    }

    osal_sem_give(s_uart_test_state.sync_sem);
}

/**
 * \brief           Test multiple tasks using UART with mutex protection
 * \details         Requirements 13.2 - Multi-task HAL usage
 */
TEST_F(HalOsalIntegrationTest, MultiTaskUartWithMutex) {
    const int NUM_BYTES = 10;
    int byte_count = NUM_BYTES;

    /* Initialize UART */
    hal_uart_config_t uart_config = {.baudrate = 115200,
                                     .wordlen = HAL_UART_WORDLEN_8,
                                     .stopbits = HAL_UART_STOPBITS_1,
                                     .parity = HAL_UART_PARITY_NONE,
                                     .flowctrl = HAL_UART_FLOWCTRL_NONE};

    ASSERT_EQ(HAL_OK, hal_uart_init(HAL_UART_0, &uart_config));

    /* Create mutex and semaphore */
    ASSERT_EQ(OSAL_OK, osal_mutex_create(&s_uart_test_state.uart_mutex));
    ASSERT_EQ(OSAL_OK, osal_sem_create(0, 2, &s_uart_test_state.sync_sem));

    /* Reset state */
    s_uart_test_state.tx_count = 0;
    s_uart_test_state.running = true;

    /* Create two transmit tasks */
    osal_task_config_t tx_config1 = {.name = "uart_tx1",
                                     .func = uart_tx_task_func,
                                     .arg = &byte_count,
                                     .priority = OSAL_TASK_PRIORITY_NORMAL,
                                     .stack_size = 4096};

    osal_task_config_t tx_config2 = {.name = "uart_tx2",
                                     .func = uart_tx_task_func,
                                     .arg = &byte_count,
                                     .priority = OSAL_TASK_PRIORITY_NORMAL,
                                     .stack_size = 4096};

    osal_task_handle_t tx_task1 = nullptr;
    osal_task_handle_t tx_task2 = nullptr;

    ASSERT_EQ(OSAL_OK, osal_task_create(&tx_config1, &tx_task1));
    ASSERT_EQ(OSAL_OK, osal_task_create(&tx_config2, &tx_task2));

    /* Wait for both tasks to complete */
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_uart_test_state.sync_sem, 5000));
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_uart_test_state.sync_sem, 5000));

    /* Stop tasks */
    s_uart_test_state.running = false;

    /* Verify bytes were transmitted */
    EXPECT_EQ(NUM_BYTES * 2, s_uart_test_state.tx_count.load());

    /* Clean up */
    osal_task_delete(tx_task1);
    osal_task_delete(tx_task2);
    osal_mutex_delete(s_uart_test_state.uart_mutex);
    osal_sem_delete(s_uart_test_state.sync_sem);
    hal_uart_deinit(HAL_UART_0);
}

/*---------------------------------------------------------------------------*/
/* Semaphore Synchronization Tests - Requirements 13.3                       */
/*---------------------------------------------------------------------------*/

static struct {
    std::atomic<int> sequence{0};
    std::atomic<bool> running{false};
    osal_sem_handle_t sem1;
    osal_sem_handle_t sem2;
    osal_sem_handle_t done_sem;
} s_sem_sync_state;

/**
 * \brief           Task A: waits on sem1, signals sem2
 */
static void sem_task_a_func(void* arg) {
    (void)arg;

    for (int i = 0; i < 5 && s_sem_sync_state.running; i++) {
        osal_sem_take(s_sem_sync_state.sem1, OSAL_WAIT_FOREVER);
        s_sem_sync_state.sequence++;
        osal_sem_give(s_sem_sync_state.sem2);
    }

    osal_sem_give(s_sem_sync_state.done_sem);
}

/**
 * \brief           Task B: waits on sem2, signals sem1
 */
static void sem_task_b_func(void* arg) {
    (void)arg;

    for (int i = 0; i < 5 && s_sem_sync_state.running; i++) {
        osal_sem_take(s_sem_sync_state.sem2, OSAL_WAIT_FOREVER);
        s_sem_sync_state.sequence++;
        osal_sem_give(s_sem_sync_state.sem1);
    }

    osal_sem_give(s_sem_sync_state.done_sem);
}

/**
 * \brief           Test semaphore-based task synchronization
 * \details         Requirements 13.3 - Inter-task communication
 */
TEST_F(HalOsalIntegrationTest, SemaphoreSynchronization) {
    /* Create semaphores */
    ASSERT_EQ(OSAL_OK,
              osal_sem_create(1, 1, &s_sem_sync_state.sem1)); /* Start with 1 */
    ASSERT_EQ(OSAL_OK,
              osal_sem_create(0, 1, &s_sem_sync_state.sem2)); /* Start with 0 */
    ASSERT_EQ(OSAL_OK, osal_sem_create(0, 2, &s_sem_sync_state.done_sem));

    /* Reset state */
    s_sem_sync_state.sequence = 0;
    s_sem_sync_state.running = true;

    /* Create tasks */
    osal_task_config_t task_a_config = {.name = "sem_task_a",
                                        .func = sem_task_a_func,
                                        .arg = nullptr,
                                        .priority = OSAL_TASK_PRIORITY_NORMAL,
                                        .stack_size = 4096};

    osal_task_config_t task_b_config = {.name = "sem_task_b",
                                        .func = sem_task_b_func,
                                        .arg = nullptr,
                                        .priority = OSAL_TASK_PRIORITY_NORMAL,
                                        .stack_size = 4096};

    osal_task_handle_t task_a = nullptr;
    osal_task_handle_t task_b = nullptr;

    ASSERT_EQ(OSAL_OK, osal_task_create(&task_a_config, &task_a));
    ASSERT_EQ(OSAL_OK, osal_task_create(&task_b_config, &task_b));

    /* Wait for both tasks to complete */
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_sem_sync_state.done_sem, 5000));
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_sem_sync_state.done_sem, 5000));

    /* Stop tasks */
    s_sem_sync_state.running = false;

    /* Verify sequence count (5 iterations * 2 tasks = 10) */
    EXPECT_EQ(10, s_sem_sync_state.sequence.load());

    /* Clean up */
    osal_task_delete(task_a);
    osal_task_delete(task_b);
    osal_sem_delete(s_sem_sync_state.sem1);
    osal_sem_delete(s_sem_sync_state.sem2);
    osal_sem_delete(s_sem_sync_state.done_sem);
}

/*---------------------------------------------------------------------------*/
/* HAL Timer with OSAL Task Tests - Requirements 13.2                        */
/*---------------------------------------------------------------------------*/

static struct {
    std::atomic<int> callback_count{0};
    std::atomic<bool> running{false};
    osal_sem_handle_t timer_sem;
} s_timer_test_state;

/**
 * \brief           Timer callback that signals semaphore
 */
static void timer_callback(hal_timer_instance_t instance, void* context) {
    (void)instance;
    (void)context;
    s_timer_test_state.callback_count++;
    osal_sem_give_from_isr(s_timer_test_state.timer_sem);
}

/**
 * \brief           Task that waits for timer events
 */
static void timer_wait_task_func(void* arg) {
    int expected_events = (arg != nullptr) ? *((int*)arg) : 5;

    for (int i = 0; i < expected_events && s_timer_test_state.running; i++) {
        osal_sem_take(s_timer_test_state.timer_sem, 1000);
    }
}

/* Forward declaration for native timer simulation helper */
extern "C" bool native_timer_simulate_period_elapsed(int instance);

/**
 * \brief           Task that simulates timer ticks for native platform
 */
static void timer_simulator_task_func(void* arg) {
    int num_ticks = (arg != nullptr) ? *((int*)arg) : 5;

    for (int i = 0; i < num_ticks && s_timer_test_state.running; i++) {
        osal_task_delay(50); /* Simulate 50ms period */
        native_timer_simulate_period_elapsed(HAL_TIMER_0);
    }
}

/**
 * \brief           Test HAL timer with OSAL task synchronization
 * \details         Requirements 13.2 - Multi-task HAL usage
 *                  Note: Native platform uses simulated timer ticks
 */
TEST_F(HalOsalIntegrationTest, TimerWithTaskSync) {
    const int NUM_EVENTS = 5;
    int event_count = NUM_EVENTS;

    /* Create semaphore */
    ASSERT_EQ(OSAL_OK, osal_sem_create(0, 10, &s_timer_test_state.timer_sem));

    /* Reset state */
    s_timer_test_state.callback_count = 0;
    s_timer_test_state.running = true;

    /* Initialize timer */
    hal_timer_config_t timer_config = {.period_us = 50000, /* 50ms */
                                       .mode = HAL_TIMER_MODE_PERIODIC,
                                       .direction = HAL_TIMER_DIR_UP};

    ASSERT_EQ(HAL_OK, hal_timer_init(HAL_TIMER_0, &timer_config));

    /* Set timer callback */
    ASSERT_EQ(HAL_OK,
              hal_timer_set_callback(HAL_TIMER_0, timer_callback, nullptr));

    /* Create waiting task */
    osal_task_config_t wait_task_config = {.name = "timer_wait",
                                           .func = timer_wait_task_func,
                                           .arg = &event_count,
                                           .priority =
                                               OSAL_TASK_PRIORITY_NORMAL,
                                           .stack_size = 4096};

    /* Create simulator task (simulates timer hardware on native platform) */
    osal_task_config_t sim_task_config = {.name = "timer_sim",
                                          .func = timer_simulator_task_func,
                                          .arg = &event_count,
                                          .priority = OSAL_TASK_PRIORITY_HIGH,
                                          .stack_size = 4096};

    osal_task_handle_t wait_task = nullptr;
    osal_task_handle_t sim_task = nullptr;

    ASSERT_EQ(OSAL_OK, osal_task_create(&wait_task_config, &wait_task));

    /* Start timer */
    ASSERT_EQ(HAL_OK, hal_timer_start(HAL_TIMER_0));

    /* Start simulator task */
    ASSERT_EQ(OSAL_OK, osal_task_create(&sim_task_config, &sim_task));

    /* Wait for events */
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    /* Stop timer and tasks */
    hal_timer_stop(HAL_TIMER_0);
    s_timer_test_state.running = false;

    /* Verify callbacks occurred */
    EXPECT_GE(s_timer_test_state.callback_count.load(), NUM_EVENTS);

    /* Clean up */
    osal_task_delete(wait_task);
    osal_task_delete(sim_task);
    osal_sem_delete(s_timer_test_state.timer_sem);
    hal_timer_deinit(HAL_TIMER_0);
}

/*---------------------------------------------------------------------------*/
/* Multiple Queues Communication Tests - Requirements 13.3                   */
/*---------------------------------------------------------------------------*/

static struct {
    osal_queue_handle_t request_queue;
    osal_queue_handle_t response_queue;
    std::atomic<int> requests_processed{0};
    std::atomic<bool> running{false};
    osal_sem_handle_t done_sem;
} s_multi_queue_state;

typedef struct {
    uint32_t request_id;
    uint32_t operation;
} request_msg_t;

typedef struct {
    uint32_t request_id;
    uint32_t result;
} response_msg_t;

/**
 * \brief           Server task: processes requests and sends responses
 */
static void server_task_func(void* arg) {
    (void)arg;

    while (s_multi_queue_state.running) {
        request_msg_t req;
        if (osal_queue_receive(s_multi_queue_state.request_queue, &req, 100) ==
            OSAL_OK) {
            /* Process request */
            response_msg_t resp = {
                .request_id = req.request_id,
                .result = req.operation * 2 /* Simple processing */
            };

            osal_queue_send(s_multi_queue_state.response_queue, &resp, 1000);
            s_multi_queue_state.requests_processed++;
        }
    }

    osal_sem_give(s_multi_queue_state.done_sem);
}

/**
 * \brief           Client task: sends requests and receives responses
 */
static void client_task_func(void* arg) {
    int num_requests = (arg != nullptr) ? *((int*)arg) : 5;

    for (int i = 0; i < num_requests && s_multi_queue_state.running; i++) {
        request_msg_t req = {.request_id = (uint32_t)i,
                             .operation = (uint32_t)(i + 10)};

        osal_queue_send(s_multi_queue_state.request_queue, &req, 1000);

        response_msg_t resp;
        if (osal_queue_receive(s_multi_queue_state.response_queue, &resp,
                               1000) == OSAL_OK) {
            EXPECT_EQ(req.request_id, resp.request_id);
            EXPECT_EQ(req.operation * 2, resp.result);
        }

        osal_task_delay(10);
    }

    osal_sem_give(s_multi_queue_state.done_sem);
}

/**
 * \brief           Test request-response pattern with multiple queues
 * \details         Requirements 13.3 - Inter-task communication
 */
TEST_F(HalOsalIntegrationTest, RequestResponseQueues) {
    const int NUM_REQUESTS = 10;
    int request_count = NUM_REQUESTS;

    /* Create queues and semaphore */
    ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(request_msg_t), 5,
                                         &s_multi_queue_state.request_queue));
    ASSERT_EQ(OSAL_OK, osal_queue_create(sizeof(response_msg_t), 5,
                                         &s_multi_queue_state.response_queue));
    ASSERT_EQ(OSAL_OK, osal_sem_create(0, 2, &s_multi_queue_state.done_sem));

    /* Reset state */
    s_multi_queue_state.requests_processed = 0;
    s_multi_queue_state.running = true;

    /* Create tasks */
    osal_task_config_t server_config = {.name = "server",
                                        .func = server_task_func,
                                        .arg = nullptr,
                                        .priority = OSAL_TASK_PRIORITY_HIGH,
                                        .stack_size = 4096};

    osal_task_config_t client_config = {.name = "client",
                                        .func = client_task_func,
                                        .arg = &request_count,
                                        .priority = OSAL_TASK_PRIORITY_NORMAL,
                                        .stack_size = 4096};

    osal_task_handle_t server = nullptr;
    osal_task_handle_t client = nullptr;

    ASSERT_EQ(OSAL_OK, osal_task_create(&server_config, &server));
    ASSERT_EQ(OSAL_OK, osal_task_create(&client_config, &client));

    /* Wait for client to complete */
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_multi_queue_state.done_sem, 5000));

    /* Stop server */
    s_multi_queue_state.running = false;
    EXPECT_EQ(OSAL_OK, osal_sem_take(s_multi_queue_state.done_sem, 1000));

    /* Verify all requests were processed */
    EXPECT_EQ(NUM_REQUESTS, s_multi_queue_state.requests_processed.load());

    /* Clean up */
    osal_task_delete(server);
    osal_task_delete(client);
    osal_queue_delete(s_multi_queue_state.request_queue);
    osal_queue_delete(s_multi_queue_state.response_queue);
    osal_sem_delete(s_multi_queue_state.done_sem);
}
