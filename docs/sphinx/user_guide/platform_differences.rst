Platform Differences
====================

Understanding platform-specific differences and how to write portable code across Nexus supported platforms.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

While Nexus provides a unified API across platforms, understanding platform differences helps write efficient and portable code.

**Supported Platforms:**

* Native (Windows/Linux/macOS)
* STM32F4 (Cortex-M4)
* STM32H7 (Cortex-M7)
* GD32 (RISC-V)
* ESP32 (Xtensa LX6)
* nRF52 (Cortex-M4F)

Platform Comparison
-------------------

Hardware Capabilities
~~~~~~~~~~~~~~~~~~~~~

+-------------+------------+----------+----------+----------+----------+
| Feature     | Native     | STM32F4  | STM32H7  | GD32     | ESP32    |
+=============+============+==========+==========+==========+==========+
| CPU         | x86/ARM64  | M4       | M7       | RISC-V   | LX6      |
+-------------+------------+----------+----------+----------+----------+
| Frequency   | Variable   | 168 MHz  | 480 MHz  | 108 MHz  | 240 MHz  |
+-------------+------------+----------+----------+----------+----------+
| Flash       | N/A        | 1 MB     | 2 MB     | 128 KB   | 4 MB     |
+-------------+------------+----------+----------+----------+----------+
| RAM         | GB         | 192 KB   | 1 MB     | 32 KB    | 520 KB   |
+-------------+------------+----------+----------+----------+----------+
| FPU         | Yes        | Yes      | Yes      | No       | Yes      |
+-------------+------------+----------+----------+----------+----------+
| DMA         | N/A        | Yes      | Yes      | Yes      | Yes      |
+-------------+------------+----------+----------+----------+----------+

Peripheral Support
~~~~~~~~~~~~~~~~~~

+-------------+--------+--------+--------+--------+--------+
| Peripheral  | Native | STM32F4| STM32H7| GD32   | ESP32  |
+=============+========+========+========+========+========+
| GPIO        | ✓      | ✓      | ✓      | ✓      | ✓      |
+-------------+--------+--------+--------+--------+--------+
| UART        | ✓      | ✓      | ✓      | ✓      | ✓      |
+-------------+--------+--------+--------+--------+--------+
| SPI         | ✓      | ✓      | ✓      | ✓      | ✓      |
+-------------+--------+--------+--------+--------+--------+
| I2C         | ✓      | ✓      | ✓      | ✓      | ✓      |
+-------------+--------+--------+--------+--------+--------+
| ADC         | ✓      | ✓      | ✓      | ✓      | ✓      |
+-------------+--------+--------+--------+--------+--------+
| PWM         | ✓      | ✓      | ✓      | ✓      | ✓      |
+-------------+--------+--------+--------+--------+--------+
| CAN         | ✗      | ✓      | ✓      | ✓      | ✗      |
+-------------+--------+--------+--------+--------+--------+
| Ethernet    | ✓      | ✗      | ✓      | ✗      | ✓      |
+-------------+--------+--------+--------+--------+--------+
| WiFi        | ✓      | ✗      | ✗      | ✗      | ✓      |
+-------------+--------+--------+--------+--------+--------+
| Bluetooth   | ✓      | ✗      | ✗      | ✗      | ✓      |
+-------------+--------+--------+--------+--------+--------+

Writing Portable Code
---------------------

Platform Detection
~~~~~~~~~~~~~~~~~~

**Compile-Time Detection:**

.. code-block:: c

   #include "nexus_config.h"

   #if defined(CONFIG_PLATFORM_NATIVE)
       /* Native platform code */
   #elif defined(CONFIG_PLATFORM_STM32F4)
       /* STM32F4 code */
   #elif defined(CONFIG_PLATFORM_STM32H7)
       /* STM32H7 code */
   #elif defined(CONFIG_PLATFORM_GD32)
       /* GD32 code */
   #elif defined(CONFIG_PLATFORM_ESP32)
       /* ESP32 code */
   #endif

**Runtime Detection:**

.. code-block:: c

   const char* get_platform_name(void)
   {
   #if defined(CONFIG_PLATFORM_NATIVE)
       return "Native";
   #elif defined(CONFIG_PLATFORM_STM32F4)
       return "STM32F4";
   #elif defined(CONFIG_PLATFORM_STM32H7)
       return "STM32H7";
   #elif defined(CONFIG_PLATFORM_GD32)
       return "GD32";
   #elif defined(CONFIG_PLATFORM_ESP32)
       return "ESP32";
   #else
       return "Unknown";
   #endif
   }

Abstraction Patterns
~~~~~~~~~~~~~~~~~~~~

**Use HAL Abstractions:**

.. code-block:: c

   /* Good: Portable code using HAL */
   void blink_led(void)
   {
       nx_gpio_write_t* led = nx_factory_gpio_write('D', 12);
       if (led) {
           led->toggle(led);
           nx_factory_gpio_release((nx_gpio_t*)led);
       }
   }

   /* Bad: Platform-specific code */
   void blink_led_bad(void)
   {
   #ifdef CONFIG_PLATFORM_STM32F4
       GPIOD->ODR ^= (1 << 12);
   #elif defined(CONFIG_PLATFORM_ESP32)
       gpio_set_level(GPIO_NUM_12, !gpio_get_level(GPIO_NUM_12));
   #endif
   }

**Platform-Specific Optimizations:**

.. code-block:: c

   void fast_memcpy(void* dest, const void* src, size_t len)
   {
   #if defined(CONFIG_PLATFORM_STM32H7)
       /* Use DMA for large transfers on STM32H7 */
       if (len > 1024) {
           dma_memcpy(dest, src, len);
           return;
       }
   #endif

       /* Fallback to standard memcpy */
       memcpy(dest, src, len);
   }

Platform-Specific Features
---------------------------

Native Platform
~~~~~~~~~~~~~~~

**Simulation Features:**

.. code-block:: c

   #ifdef CONFIG_PLATFORM_NATIVE

   /* File system access */
   #include <stdio.h>

   void save_config(const char* filename)
   {
       FILE* f = fopen(filename, "w");
       if (f) {
           fprintf(f, "config_value=42\n");
           fclose(f);
       }
   }

   /* Network simulation */
   #include <sys/socket.h>

   int connect_to_server(const char* host, int port)
   {
       /* Standard socket API */
       int sock = socket(AF_INET, SOCK_STREAM, 0);
       /* ... */
       return sock;
   }

   #endif

STM32 Platforms
~~~~~~~~~~~~~~~

**STM32-Specific Features:**

.. code-block:: c

   #if defined(CONFIG_PLATFORM_STM32F4) || defined(CONFIG_PLATFORM_STM32H7)

   /* Access unique device ID */
   uint32_t get_device_id(void)
   {
       return *(uint32_t*)0x1FFF7A10;
   }

   /* Use hardware CRC */
   uint32_t calculate_crc32(const uint8_t* data, size_t len)
   {
       __HAL_RCC_CRC_CLK_ENABLE();
       CRC->CR = CRC_CR_RESET;

       for (size_t i = 0; i < len / 4; i++) {
           CRC->DR = ((uint32_t*)data)[i];
       }

       return CRC->DR;
   }

   #endif

ESP32 Platform
~~~~~~~~~~~~~~

**ESP32-Specific Features:**

.. code-block:: c

   #ifdef CONFIG_PLATFORM_ESP32

   /* WiFi functionality */
   #include "esp_wifi.h"

   void connect_wifi(const char* ssid, const char* password)
   {
       wifi_config_t wifi_config = {
           .sta = {
               .ssid = "",
               .password = "",
           },
       };

       strcpy((char*)wifi_config.sta.ssid, ssid);
       strcpy((char*)wifi_config.sta.password, password);

       esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
       esp_wifi_connect();
   }

   /* Bluetooth functionality */
   #include "esp_bt.h"

   void init_bluetooth(void)
   {
       esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
       esp_bt_controller_init(&bt_cfg);
       esp_bt_controller_enable(ESP_BT_MODE_BLE);
   }

   #endif

Performance Considerations
--------------------------

CPU Performance
~~~~~~~~~~~~~~~

**Relative Performance:**

.. code-block:: c

   /* Benchmark results (relative to Native = 1.0) */
   Platform    | Integer | Float | Memory
   ------------|---------|-------|--------
   Native      | 1.00    | 1.00  | 1.00
   STM32H7     | 0.15    | 0.20  | 0.10
   STM32F4     | 0.08    | 0.10  | 0.08
   ESP32       | 0.12    | 0.15  | 0.12
   GD32        | 0.06    | N/A   | 0.05

**Optimization Strategies:**

.. code-block:: c

   /* Adjust algorithm based on platform */
   void process_data(const uint8_t* data, size_t len)
   {
   #ifdef CONFIG_PLATFORM_NATIVE
       /* Use complex algorithm on powerful platform */
       advanced_processing(data, len);
   #else
       /* Use simpler algorithm on embedded platforms */
       basic_processing(data, len);
   #endif
   }

Memory Constraints
~~~~~~~~~~~~~~~~~~

**Memory-Aware Code:**

.. code-block:: c

   /* Adjust buffer sizes based on platform */
   #if defined(CONFIG_PLATFORM_NATIVE)
       #define BUFFER_SIZE 65536
   #elif defined(CONFIG_PLATFORM_STM32H7)
       #define BUFFER_SIZE 8192
   #elif defined(CONFIG_PLATFORM_STM32F4)
       #define BUFFER_SIZE 2048
   #elif defined(CONFIG_PLATFORM_GD32)
       #define BUFFER_SIZE 512
   #endif

   static uint8_t buffer[BUFFER_SIZE];

**Dynamic Allocation:**

.. code-block:: c

   void* allocate_buffer(size_t size)
   {
   #ifdef CONFIG_PLATFORM_GD32
       /* Limited RAM - check carefully */
       if (size > 1024) {
           LOG_ERROR("Allocation too large for GD32");
           return NULL;
       }
   #endif

       return osal_malloc(size);
   }

Timing and Delays
-----------------

Tick Resolution
~~~~~~~~~~~~~~~

**Platform Tick Rates:**

.. code-block:: c

   /* Typical tick rates */
   Platform    | Tick Rate | Resolution
   ------------|-----------|------------
   Native      | 1000 Hz   | 1 ms
   STM32F4     | 1000 Hz   | 1 ms
   STM32H7     | 1000 Hz   | 1 ms
   ESP32       | 100 Hz    | 10 ms
   GD32        | 1000 Hz   | 1 ms

**Portable Timing:**

.. code-block:: c

   /* Use OSAL for portable delays */
   void delay_ms(uint32_t ms)
   {
       osal_task_delay(ms);
   }

   /* High-resolution timing when needed */
   uint32_t get_microseconds(void)
   {
   #if defined(CONFIG_PLATFORM_NATIVE)
       struct timespec ts;
       clock_gettime(CLOCK_MONOTONIC, &ts);
       return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
   #else
       /* Use hardware timer */
       return hal_timer_get_us();
   #endif
   }

Interrupt Handling
------------------

Interrupt Priorities
~~~~~~~~~~~~~~~~~~~~

**Platform Differences:**

.. code-block:: c

   /* STM32: 16 priority levels (0-15, lower = higher priority) */
   #ifdef CONFIG_PLATFORM_STM32F4
       #define IRQ_PRIORITY_HIGHEST    0
       #define IRQ_PRIORITY_HIGH       4
       #define IRQ_PRIORITY_NORMAL     8
       #define IRQ_PRIORITY_LOW        12
       #define IRQ_PRIORITY_LOWEST     15
   #endif

   /* ESP32: 7 priority levels (1-7, higher = higher priority) */
   #ifdef CONFIG_PLATFORM_ESP32
       #define IRQ_PRIORITY_HIGHEST    7
       #define IRQ_PRIORITY_HIGH       5
       #define IRQ_PRIORITY_NORMAL     3
       #define IRQ_PRIORITY_LOW        2
       #define IRQ_PRIORITY_LOWEST     1
   #endif

**Portable Interrupt Configuration:**

.. code-block:: c

   void configure_interrupt(int irq_num, int priority)
   {
   #if defined(CONFIG_PLATFORM_STM32F4)
       NVIC_SetPriority(irq_num, priority);
       NVIC_EnableIRQ(irq_num);
   #elif defined(CONFIG_PLATFORM_ESP32)
       esp_intr_alloc(irq_num, priority, isr_handler, NULL, NULL);
   #endif
   }

Power Management
----------------

Sleep Modes
~~~~~~~~~~~

**Platform Sleep Modes:**

.. code-block:: c

   typedef enum {
       POWER_MODE_RUN,
       POWER_MODE_SLEEP,
       POWER_MODE_STOP,
       POWER_MODE_STANDBY,
   } power_mode_t;

   void enter_low_power_mode(power_mode_t mode)
   {
   #if defined(CONFIG_PLATFORM_STM32F4)
       switch (mode) {
       case POWER_MODE_SLEEP:
           HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
           break;
       case POWER_MODE_STOP:
           HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
           break;
       case POWER_MODE_STANDBY:
           HAL_PWR_EnterSTANDBYMode();
           break;
       }
   #elif defined(CONFIG_PLATFORM_ESP32)
       switch (mode) {
       case POWER_MODE_SLEEP:
           esp_light_sleep_start();
           break;
       case POWER_MODE_STOP:
           esp_deep_sleep_start();
           break;
       }
   #endif
   }

Testing Across Platforms
-------------------------

Unit Testing
~~~~~~~~~~~~

**Platform-Specific Tests:**

.. code-block:: cpp

   #ifdef CONFIG_PLATFORM_NATIVE

   TEST(PlatformTest, NativeFeatures) {
       /* Test native-specific features */
       EXPECT_TRUE(file_system_available());
   }

   #endif

   #ifdef CONFIG_PLATFORM_STM32F4

   TEST(PlatformTest, STM32Features) {
       /* Test STM32-specific features */
       uint32_t device_id = get_device_id();
       EXPECT_NE(device_id, 0);
   }

   #endif

**Cross-Platform Tests:**

.. code-block:: cpp

   TEST(PortableTest, GPIOOperations) {
       /* Test works on all platforms */
       nx_gpio_write_t* gpio = nx_factory_gpio_write('A', 0);
       ASSERT_NE(gpio, nullptr);

       gpio->write(gpio, 1);
       EXPECT_EQ(gpio->read(gpio), 1);

       nx_factory_gpio_release((nx_gpio_t*)gpio);
   }

Best Practices
--------------

1. **Use HAL Abstractions**
   * Prefer HAL APIs over direct register access
   * Use factory pattern for device creation
   * Release resources properly

2. **Handle Platform Differences**
   * Use compile-time detection
   * Provide fallbacks
   * Document platform-specific code

3. **Test on Target Hardware**
   * Native testing is not sufficient
   * Test on actual hardware
   * Verify timing and performance

4. **Consider Resource Constraints**
   * Adjust buffer sizes
   * Optimize for memory
   * Profile on target

5. **Document Platform Requirements**
   * Specify minimum requirements
   * Document platform-specific features
   * Provide porting guide

See Also
--------

* :doc:`../platform_guides/index` - Platform-Specific Guides
* :doc:`porting` - Porting Guide
* :doc:`performance` - Performance Optimization
* :doc:`best_practices` - Best Practices

