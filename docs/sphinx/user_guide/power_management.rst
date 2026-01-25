Power Management
================

Comprehensive guide to power management and optimization for Nexus embedded applications.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Power management is critical for battery-powered embedded systems. This guide covers power optimization strategies, sleep modes, and energy-efficient design.

**Power Management Goals:**

* Minimize power consumption
* Extend battery life
* Reduce heat generation
* Meet power budgets
* Maintain performance requirements

**Power Consumption Sources:**

* CPU execution
* Peripheral operation
* Memory access
* Clock generation
* I/O pin states
* Leakage current

Power Modes
-----------

ARM Cortex-M Power Modes
~~~~~~~~~~~~~~~~~~~~~~~~~

**Available Modes:**

+-------------------+------------------+------------------+------------------+
| Mode              | CPU              | Peripherals      | Wake-up Time     |
+===================+==================+==================+==================+
| Run               | Active           | Active           | N/A              |
+-------------------+------------------+------------------+------------------+
| Sleep             | Stopped          | Active           | Fast (<1µs)      |
+-------------------+------------------+------------------+------------------+
| Stop              | Stopped          | Stopped          | Medium (5-50µs)  |
+-------------------+------------------+------------------+------------------+
| Standby           | Off              | Off              | Slow (50-500µs)  |
+-------------------+------------------+------------------+------------------+

**Power Consumption (Typical STM32F4):**

* Run Mode: 50-100 mA @ 168 MHz
* Sleep Mode: 30-50 mA
* Stop Mode: 100-500 µA
* Standby Mode: 1-10 µA

Sleep Mode
~~~~~~~~~~

**Enter Sleep Mode:**

.. code-block:: c

   #include "hal/nx_power.h"

   void enter_sleep_mode(void)
   {
       /* Configure wake-up sources */
       nx_power_config_wakeup(WAKEUP_UART | WAKEUP_GPIO);

       /* Enter sleep mode */
       nx_power_enter_sleep();

       /* CPU stops here until interrupt */

       /* Resume execution after wake-up */
       LOG_INFO("Woke up from sleep");
   }

**Sleep-on-Exit (Cortex-M):**

.. code-block:: c

   /* Enable sleep-on-exit for interrupt-driven systems */
   void enable_sleep_on_exit(void)
   {
       SCB->SCR |= SCB_SCR_SLEEPONEXIT_Msk;

       /* CPU sleeps after returning from ISR */
       /* Wakes up on next interrupt */
   }

**FreeRTOS Idle Hook:**

.. code-block:: c

   void vApplicationIdleHook(void)
   {
       /* Enter sleep when idle */
       __WFI();  /* Wait For Interrupt */
   }

Stop Mode
~~~~~~~~~

**Enter Stop Mode:**

.. code-block:: c

   void enter_stop_mode(void)
   {
       /* Save peripheral state */
       save_peripheral_state();

       /* Configure wake-up sources */
       nx_power_config_wakeup(WAKEUP_RTC | WAKEUP_BUTTON);

       /* Enter stop mode */
       nx_power_enter_stop();

       /* CPU and most peripherals stopped */
       /* Wake up on configured interrupt */

       /* Restore peripheral state */
       restore_peripheral_state();

       LOG_INFO("Woke up from stop mode");
   }

**RTC Wake-up:**

.. code-block:: c

   void setup_rtc_wakeup(uint32_t seconds)
   {
       nx_rtc_t* rtc = nx_factory_rtc(0);

       /* Configure RTC wake-up timer */
       nx_rtc_wakeup_config_t config = {
           .period = seconds,
           .callback = rtc_wakeup_callback,
           .user_data = NULL,
       };

       rtc->configure_wakeup(rtc, &config);
       rtc->enable_wakeup(rtc);

       nx_factory_rtc_release(rtc);
   }

   void rtc_wakeup_callback(void* user_data)
   {
       LOG_INFO("RTC wake-up triggered");
   }

Standby Mode
~~~~~~~~~~~~

**Enter Standby Mode:**

.. code-block:: c

   void enter_standby_mode(void)
   {
       /* Save critical data to backup SRAM or Flash */
       save_critical_data();

       /* Configure wake-up pin */
       nx_power_config_wakeup_pin(WAKEUP_PIN_1, RISING_EDGE);

       /* Enter standby mode */
       nx_power_enter_standby();

       /* System resets on wake-up */
       /* Execution starts from reset vector */
   }

**Check Wake-up Source:**

.. code-block:: c

   void check_wakeup_source(void)
   {
       if (nx_power_is_wakeup_from_standby()) {
           uint32_t source = nx_power_get_wakeup_source();

           if (source & WAKEUP_PIN_1) {
               LOG_INFO("Woke up from button press");
           } else if (source & WAKEUP_RTC) {
               LOG_INFO("Woke up from RTC alarm");
           }

           /* Restore critical data */
           restore_critical_data();
       }
   }

Clock Management
----------------

Dynamic Frequency Scaling
~~~~~~~~~~~~~~~~~~~~~~~~~

**Adjust Clock Frequency:**

.. code-block:: c

   typedef enum {
       CLOCK_SPEED_LOW = 0,      /* 16 MHz */
       CLOCK_SPEED_MEDIUM,       /* 84 MHz */
       CLOCK_SPEED_HIGH,         /* 168 MHz */
   } clock_speed_t;

   void set_clock_speed(clock_speed_t speed)
   {
       switch (speed) {
       case CLOCK_SPEED_LOW:
           /* Configure PLL for 16 MHz */
           nx_clock_set_frequency(16000000);
           LOG_INFO("Clock: 16 MHz (low power)");
           break;

       case CLOCK_SPEED_MEDIUM:
           /* Configure PLL for 84 MHz */
           nx_clock_set_frequency(84000000);
           LOG_INFO("Clock: 84 MHz (balanced)");
           break;

       case CLOCK_SPEED_HIGH:
           /* Configure PLL for 168 MHz */
           nx_clock_set_frequency(168000000);
           LOG_INFO("Clock: 168 MHz (high performance)");
           break;
       }

       /* Update SystemCoreClock variable */
       SystemCoreClockUpdate();

       /* Reconfigure peripherals if needed */
       reconfigure_peripherals();
   }

**Adaptive Frequency:**

.. code-block:: c

   void adaptive_frequency_task(void* arg)
   {
       while (1) {
           uint32_t cpu_usage = get_cpu_usage_percent();

           if (cpu_usage > 80) {
               /* High load - increase frequency */
               set_clock_speed(CLOCK_SPEED_HIGH);
           } else if (cpu_usage < 20) {
               /* Low load - decrease frequency */
               set_clock_speed(CLOCK_SPEED_LOW);
           } else {
               /* Medium load - balanced frequency */
               set_clock_speed(CLOCK_SPEED_MEDIUM);
           }

           osal_task_delay(1000);
       }
   }

Peripheral Clock Gating
~~~~~~~~~~~~~~~~~~~~~~~

**Disable Unused Peripherals:**

.. code-block:: c

   void disable_unused_peripherals(void)
   {
       /* Disable unused peripheral clocks */
       nx_clock_disable_peripheral(PERIPHERAL_SPI2);
       nx_clock_disable_peripheral(PERIPHERAL_I2C2);
       nx_clock_disable_peripheral(PERIPHERAL_USART3);
       nx_clock_disable_peripheral(PERIPHERAL_TIM3);
       nx_clock_disable_peripheral(PERIPHERAL_TIM4);

       LOG_INFO("Disabled unused peripheral clocks");
   }

**Dynamic Clock Control:**

.. code-block:: c

   void use_spi_with_clock_control(void)
   {
       /* Enable SPI clock before use */
       nx_clock_enable_peripheral(PERIPHERAL_SPI1);

       /* Use SPI */
       nx_spi_t* spi = nx_factory_spi(0);
       spi->transfer(spi, tx_data, rx_data, len, 1000);
       nx_factory_spi_release(spi);

       /* Disable SPI clock after use */
       nx_clock_disable_peripheral(PERIPHERAL_SPI1);
   }

Peripheral Power Management
----------------------------

GPIO Power Optimization
~~~~~~~~~~~~~~~~~~~~~~~

**Configure GPIO for Low Power:**

.. code-block:: c

   void configure_gpio_low_power(void)
   {
       /* Set unused pins to analog input (lowest power) */
       for (uint8_t pin = 0; pin < 16; pin++) {
           if (!is_pin_used('A', pin)) {
               nx_gpio_t* gpio = nx_factory_gpio('A', pin);
               gpio->set_mode(gpio, NX_GPIO_MODE_ANALOG);
               nx_factory_gpio_release(gpio);
           }
       }

       /* Disable pull-ups/pull-downs on unused pins */
       /* Configure output pins to known state */
   }

**Output Pin States:**

.. code-block:: c

   void set_output_pins_low_power(void)
   {
       /* Set output pins to low to minimize current */
       nx_gpio_write_t* led = nx_factory_gpio_write('D', 12);
       led->write(led, 0);  /* Turn off LED */
       nx_factory_gpio_release((nx_gpio_t*)led);

       /* Or configure as input to save power */
       nx_gpio_t* gpio = nx_factory_gpio('D', 13);
       gpio->set_mode(gpio, NX_GPIO_MODE_INPUT);
       nx_factory_gpio_release(gpio);
   }

UART Power Management
~~~~~~~~~~~~~~~~~~~~~

**UART Sleep Mode:**

.. code-block:: c

   void uart_power_management(void)
   {
       nx_uart_t* uart = nx_factory_uart(0);

       /* Use UART */
       nx_tx_sync_t* tx = uart->get_tx_sync(uart);
       tx->send(tx, data, len, 1000);

       /* Enter low power mode when idle */
       nx_power_t* power = uart->get_power(uart);
       power->set_mode(power, NX_POWER_MODE_SLEEP);

       /* UART wakes up automatically on RX activity */

       nx_factory_uart_release(uart);
   }

DMA for Power Savings
~~~~~~~~~~~~~~~~~~~~~

**Use DMA to Allow CPU Sleep:**

.. code-block:: c

   void uart_send_with_dma_sleep(nx_uart_t* uart,
                                  const uint8_t* data,
                                  size_t len)
   {
       /* Start DMA transfer */
       nx_tx_dma_t* tx_dma = uart->get_tx_dma(uart);
       tx_dma->send(tx_dma, data, len);

       /* CPU can sleep while DMA transfers data */
       while (!tx_dma->is_complete(tx_dma)) {
           __WFI();  /* Sleep until DMA interrupt */
       }

       LOG_INFO("DMA transfer complete");
   }

Battery Monitoring
------------------

Battery Voltage Measurement
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Measure Battery Voltage:**

.. code-block:: c

   float measure_battery_voltage(void)
   {
       nx_adc_t* adc = nx_factory_adc(0);

       /* Configure ADC for battery measurement */
       nx_adc_config_t config = {
           .resolution = ADC_RESOLUTION_12BIT,
           .sampling_time = ADC_SAMPLING_480_CYCLES,
       };
       adc->configure(adc, &config);

       /* Read battery voltage (with voltage divider) */
       uint16_t raw_value = adc->read(adc, ADC_CHANNEL_VBAT);

       /* Convert to voltage (assuming 12-bit ADC, 3.3V ref) */
       float voltage = (raw_value * 3.3f) / 4096.0f;

       /* Account for voltage divider (if used) */
       voltage *= 2.0f;  /* Example: 1:2 divider */

       nx_factory_adc_release(adc);

       return voltage;
   }

**Battery Level Estimation:**

.. code-block:: c

   typedef enum {
       BATTERY_CRITICAL = 0,  /* < 10% */
       BATTERY_LOW,           /* 10-25% */
       BATTERY_MEDIUM,        /* 25-75% */
       BATTERY_HIGH,          /* > 75% */
   } battery_level_t;

   battery_level_t get_battery_level(void)
   {
       float voltage = measure_battery_voltage();

       /* LiPo battery voltage ranges */
       const float V_MIN = 3.0f;   /* Empty */
       const float V_MAX = 4.2f;   /* Full */

       if (voltage < V_MIN) {
           return BATTERY_CRITICAL;
       }

       float percent = ((voltage - V_MIN) / (V_MAX - V_MIN)) * 100.0f;

       if (percent < 10.0f) {
           return BATTERY_CRITICAL;
       } else if (percent < 25.0f) {
           return BATTERY_LOW;
       } else if (percent < 75.0f) {
           return BATTERY_MEDIUM;
       } else {
           return BATTERY_HIGH;
       }
   }

**Battery Monitoring Task:**

.. code-block:: c

   void battery_monitor_task(void* arg)
   {
       while (1) {
           float voltage = measure_battery_voltage();
           battery_level_t level = get_battery_level();

           LOG_INFO("Battery: %.2fV (%s)",
                    voltage, battery_level_to_string(level));

           if (level == BATTERY_CRITICAL) {
               LOG_WARN("Battery critical - entering low power mode");
               enter_emergency_low_power_mode();
           } else if (level == BATTERY_LOW) {
               LOG_WARN("Battery low - reducing power consumption");
               reduce_power_consumption();
           }

           /* Check every 60 seconds */
           osal_task_delay(60000);
       }
   }

Power Optimization Strategies
------------------------------

Interrupt-Driven Design
~~~~~~~~~~~~~~~~~~~~~~~

**Avoid Polling:**

.. code-block:: c

   /* Bad: Polling wastes power */
   void check_button_polling(void)
   {
       while (1) {
           if (read_button() == PRESSED) {
               handle_button_press();
           }
           osal_task_delay(10);  /* Still wastes power */
       }
   }

   /* Good: Interrupt-driven */
   void button_interrupt_handler(void* ctx)
   {
       handle_button_press();
   }

   void setup_button_interrupt(void)
   {
       nx_gpio_t* button = nx_factory_gpio('A', 0);
       button->set_mode(button, NX_GPIO_MODE_INPUT);
       button->set_exti(button, NX_GPIO_EXTI_FALLING,
                        button_interrupt_handler, NULL);
       nx_factory_gpio_release(button);

       /* CPU can sleep until button press */
   }

Efficient Task Scheduling
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Consolidate Wake-ups:**

.. code-block:: c

   /* Bad: Multiple tasks with different periods */
   void sensor_task_1(void* arg)
   {
       while (1) {
           read_sensor_1();
           osal_task_delay(100);  /* Wake every 100ms */
       }
   }

   void sensor_task_2(void* arg)
   {
       while (1) {
           read_sensor_2();
           osal_task_delay(150);  /* Wake every 150ms */
       }
   }

   /* Good: Single task with synchronized periods */
   void sensor_task_combined(void* arg)
   {
       uint32_t count = 0;

       while (1) {
           if (count % 100 == 0) {
               read_sensor_1();
           }
           if (count % 150 == 0) {
               read_sensor_2();
           }

           count += 50;
           osal_task_delay(50);  /* Single wake-up period */
       }
   }

Batch Processing
~~~~~~~~~~~~~~~~

**Batch Operations:**

.. code-block:: c

   /* Bad: Process data immediately */
   void data_callback(uint8_t byte)
   {
       process_single_byte(byte);  /* Frequent wake-ups */
   }

   /* Good: Batch processing */
   #define BATCH_SIZE 64
   static uint8_t batch_buffer[BATCH_SIZE];
   static size_t batch_count = 0;

   void data_callback_batched(uint8_t byte)
   {
       batch_buffer[batch_count++] = byte;

       if (batch_count >= BATCH_SIZE) {
           /* Process entire batch at once */
           process_batch(batch_buffer, batch_count);
           batch_count = 0;
       }
   }

Tickless Idle
~~~~~~~~~~~~~

**FreeRTOS Tickless Idle:**

.. code-block:: c

   /* Enable tickless idle in FreeRTOSConfig.h */
   #define configUSE_TICKLESS_IDLE 1

   /* Implement tickless idle hook */
   void vApplicationSleep(TickType_t xExpectedIdleTime)
   {
       /* Calculate sleep duration */
       uint32_t sleep_ms = xExpectedIdleTime * portTICK_PERIOD_MS;

       /* Configure wake-up timer */
       setup_wakeup_timer(sleep_ms);

       /* Enter low power mode */
       enter_stop_mode();

       /* Adjust tick count after wake-up */
       vTaskStepTick(xExpectedIdleTime);
   }

Power Profiling
---------------

Measuring Power Consumption
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Hardware Setup:**

* Power supply with current measurement
* Multimeter or oscilloscope
* Current sense resistor
* Data logging

**Software Profiling:**

.. code-block:: c

   void profile_power_modes(void)
   {
       LOG_INFO("Starting power profiling...");

       /* Measure run mode */
       LOG_INFO("Run mode - measure current now");
       osal_task_delay(5000);

       /* Measure sleep mode */
       LOG_INFO("Sleep mode - measure current now");
       for (int i = 0; i < 50; i++) {
           __WFI();
           osal_task_delay(100);
       }

       /* Measure stop mode */
       LOG_INFO("Stop mode - measure current now");
       enter_stop_mode();
       osal_task_delay(5000);

       LOG_INFO("Power profiling complete");
   }

Energy Calculation
~~~~~~~~~~~~~~~~~~

**Calculate Energy Consumption:**

.. code-block:: c

   typedef struct {
       const char* mode;
       float current_ma;
       uint32_t duration_ms;
   } power_profile_t;

   float calculate_energy(power_profile_t* profile, size_t count)
   {
       float total_energy_mah = 0.0f;

       for (size_t i = 0; i < count; i++) {
           /* Energy (mAh) = Current (mA) × Time (h) */
           float time_hours = profile[i].duration_ms / 3600000.0f;
           float energy = profile[i].current_ma * time_hours;
           total_energy_mah += energy;

           LOG_INFO("%s: %.2f mA × %.2f h = %.4f mAh",
                    profile[i].mode,
                    profile[i].current_ma,
                    time_hours,
                    energy);
       }

       return total_energy_mah;
   }

   void estimate_battery_life(void)
   {
       power_profile_t profile[] = {
           {"Active",  50.0f, 1000},    /* 1s active */
           {"Sleep",   5.0f,  9000},    /* 9s sleep */
       };

       /* Calculate energy per cycle (10 seconds) */
       float energy_per_cycle = calculate_energy(profile, 2);

       /* Calculate daily energy */
       float cycles_per_day = (24.0f * 3600.0f) / 10.0f;
       float energy_per_day = energy_per_cycle * cycles_per_day;

       /* Estimate battery life */
       float battery_capacity = 2000.0f;  /* 2000 mAh */
       float battery_life_days = battery_capacity / energy_per_day;

       LOG_INFO("Estimated battery life: %.1f days", battery_life_days);
   }

Best Practices
--------------

1. **Design for Low Power from Start**
   * Choose low-power MCU
   * Plan power modes
   * Design for sleep
   * Consider battery capacity

2. **Use Appropriate Power Modes**
   * Sleep for short idle periods
   * Stop for medium idle periods
   * Standby for long idle periods
   * Match mode to wake-up latency requirements

3. **Optimize Clock Usage**
   * Use lowest frequency that meets requirements
   * Disable unused peripheral clocks
   * Use dynamic frequency scaling
   * Consider external oscillator power

4. **Minimize Active Time**
   * Process data efficiently
   * Use DMA for transfers
   * Batch operations
   * Use hardware acceleration

5. **Configure Peripherals Properly**
   * Disable unused peripherals
   * Use low-power peripheral modes
   * Configure GPIO for low power
   * Use pull-ups/pull-downs appropriately

6. **Monitor and Measure**
   * Measure actual power consumption
   * Profile different modes
   * Calculate battery life
   * Optimize based on measurements

7. **Test in Real Conditions**
   * Test with actual battery
   * Test temperature effects
   * Test aging effects
   * Verify wake-up reliability

Power Optimization Checklist
-----------------------------

**Hardware:**

- [ ] Low-power MCU selected
- [ ] Appropriate voltage regulator
- [ ] Battery capacity adequate
- [ ] Power measurement capability

**Software:**

- [ ] Sleep modes implemented
- [ ] Unused peripherals disabled
- [ ] GPIO configured for low power
- [ ] Interrupt-driven design
- [ ] DMA used where appropriate

**Testing:**

- [ ] Power consumption measured
- [ ] Battery life calculated
- [ ] Wake-up latency verified
- [ ] Temperature effects tested

**Optimization:**

- [ ] Clock frequency optimized
- [ ] Active time minimized
- [ ] Batch processing implemented
- [ ] Tickless idle enabled

See Also
--------

* :doc:`performance` - Performance Optimization
* :doc:`memory_management` - Memory Management
* :doc:`best_practices` - Best Practices
* :doc:`../platform_guides/index` - Platform-Specific Power Features

