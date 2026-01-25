UART Communication Tutorial
===========================

This tutorial teaches you how to use UART (Universal Asynchronous Receiver/Transmitter) for serial communication with the Nexus HAL. You'll learn how to send and receive data, implement protocols, and debug your applications.

Learning Objectives
-------------------

By the end of this tutorial, you will:

- Configure UART peripherals with proper settings
- Send and receive data over UART
- Implement simple communication protocols
- Handle UART errors and timeouts
- Use UART for debugging and logging

Prerequisites
-------------

- Completed :doc:`first_application` and :doc:`gpio_control` tutorials
- STM32F4 Discovery board or compatible hardware
- USB-to-Serial adapter (FTDI, CP2102, etc.) or ST-Link virtual COM port
- Serial terminal software (PuTTY, minicom, screen, etc.)

Hardware Setup
--------------

**STM32F4 Discovery UART2 Pins:**

- PA2: UART2 TX (transmit)
- PA3: UART2 RX (receive)
- GND: Ground

**Wiring:**

Connect your USB-to-Serial adapter:

.. code-block:: text

    STM32F4          USB-Serial
    -------          ----------
    PA2 (TX)    -->  RX
    PA3 (RX)    <--  TX
    GND         ---  GND

.. warning::
   Make sure voltage levels match! STM32F4 uses 3.3V logic. Most USB-Serial adapters support 3.3V, but verify before connecting.

**Serial Terminal Settings:**

- Baud rate: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None
- Flow control: None

Part 1: Basic UART Transmission
-------------------------------

Let's start by sending data over UART.

UART Communication Workflow
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following diagram shows the complete UART communication workflow:

.. mermaid::
   :alt: UART communication workflow showing initialization, transmission, and reception

   flowchart TD
       START([Start]) --> INIT_HAL[Initialize HAL]
       INIT_HAL --> CONFIG[Configure UART Settings]
       CONFIG --> INIT_UART[Initialize UART Peripheral]
       INIT_UART --> CHECK{Init Success?}

       CHECK -->|No| ERROR[Error Handler]
       ERROR --> BLINK[Blink Error LED]
       BLINK --> ERROR

       CHECK -->|Yes| READY[UART Ready]
       READY --> TRANSMIT[Transmit Data]
       TRANSMIT --> WAIT_TX{TX Complete?}

       WAIT_TX -->|Timeout| TX_ERROR[Handle TX Error]
       WAIT_TX -->|Success| RECEIVE[Receive Data]

       RECEIVE --> WAIT_RX{RX Complete?}
       WAIT_RX -->|Timeout| RX_ERROR[Handle RX Error]
       WAIT_RX -->|Success| PROCESS[Process Received Data]

       TX_ERROR --> READY
       RX_ERROR --> READY
       PROCESS --> READY

       style START fill:#e1f5ff
       style INIT_HAL fill:#fff4e1
       style CONFIG fill:#ffe1f5
       style INIT_UART fill:#ffe1f5
       style TRANSMIT fill:#e1ffe1
       style RECEIVE fill:#e1ffe1
       style ERROR fill:#ffcccc
       style TX_ERROR fill:#ffcccc
       style RX_ERROR fill:#ffcccc

Simple "Hello World"
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    #include "hal/hal.h"
    #include <string.h>

    #define UART_ID  HAL_UART_1  /* UART1 or UART2 depending on platform */

    int main(void) {
        /* Initialize HAL */
        hal_init();

        /* Configure UART */
        hal_uart_config_t uart_config = {
            .baudrate = 115200,
            .wordlen = HAL_UART_WORDLEN_8,
            .stopbits = HAL_UART_STOPBITS_1,
            .parity = HAL_UART_PARITY_NONE,
            .flowctrl = HAL_UART_FLOWCTRL_NONE
        };

        /* Initialize UART */
        if (hal_uart_init(UART_ID, &uart_config) != HAL_OK) {
            /* Error: blink LED */
            while (1) {
                hal_gpio_toggle(HAL_GPIO_PORT_D, 14);  /* Red LED */
                hal_delay_ms(100);
            }
        }

        /* Send message */
        const char* message = "Hello, Nexus!\r\n";
        hal_uart_write(UART_ID, (const uint8_t*)message, strlen(message), 1000);

        /* Main loop */
        while (1) {
            hal_delay_ms(1000);
        }

        return 0;
    }

**Key Points:**

- ``hal_uart_init()`` configures the UART peripheral
- ``hal_uart_write()`` sends data with a timeout (in milliseconds)
- Use ``\r\n`` for proper line endings in terminal

Sending Formatted Output
~~~~~~~~~~~~~~~~~~~~~~~~

Create a printf-like function for UART:

.. code-block:: c

    #include <stdio.h>
    #include <stdarg.h>

    /**
     * \brief           Print formatted string to UART
     * \param[in]       fmt: Format string (printf-style)
     * \param[in]       ...: Variable arguments
     */
    static void uart_printf(const char* fmt, ...) {
        char buffer[128];
        va_list args;

        va_start(args, fmt);
        int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        if (len > 0) {
            hal_uart_write(UART_ID, (uint8_t*)buffer, len, 1000);
        }
    }

    /* Usage */
    int main(void) {
        hal_init();

        hal_uart_config_t config = {
            .baudrate = 115200,
            .wordlen = HAL_UART_WORDLEN_8,
            .stopbits = HAL_UART_STOPBITS_1,
            .parity = HAL_UART_PARITY_NONE,
            .flowctrl = HAL_UART_FLOWCTRL_NONE
        };
        hal_uart_init(UART_ID, &config);

        /* Print various data types */
        uart_printf("System initialized\r\n");
        uart_printf("Tick: %lu ms\r\n", (unsigned long)hal_get_tick());
        uart_printf("Temperature: %.2f C\r\n", 25.5);
        uart_printf("Status: 0x%08X\r\n", 0x12345678);

        while (1) {
            hal_delay_ms(1000);
        }

        return 0;
    }

Part 2: Receiving UART Data
---------------------------

Blocking Receive
~~~~~~~~~~~~~~~~

Receive data with a timeout:

.. code-block:: c

    /**
     * \brief           Receive and echo data
     */
    static void uart_echo_demo(void) {
        uint8_t rx_byte;
        hal_status_t status;

        uart_printf("Echo demo - type something:\r\n");

        while (1) {
            /* Receive one byte with 1 second timeout */
            status = hal_uart_read(UART_ID, &rx_byte, 1, 1000);

            if (status == HAL_OK) {
                /* Echo received byte */
                hal_uart_write(UART_ID, &rx_byte, 1, 100);

                /* Check for Enter key */
                if (rx_byte == '\r' || rx_byte == '\n') {
                    uart_printf("\r\n");
                }
            } else if (status == HAL_ERR_TIMEOUT) {
                /* Timeout - no data received */
                uart_printf(".");  /* Heartbeat */
            } else {
                /* Error */
                uart_printf("Error: %d\r\n", status);
            }
        }
    }

Line-Based Input
~~~~~~~~~~~~~~~~

Receive complete lines of text:

.. code-block:: c

    #define LINE_BUFFER_SIZE  128

    /**
     * \brief           Read a line from UART
     * \param[out]      buffer: Buffer to store line
     * \param[in]       size: Buffer size
     * \param[in]       timeout_ms: Timeout in milliseconds
     * \return          Number of characters read, or -1 on error
     */
    static int uart_read_line(char* buffer, size_t size, uint32_t timeout_ms) {
        size_t index = 0;
        uint32_t start_time = hal_get_tick();

        while (index < (size - 1)) {
            /* Check timeout */
            if ((hal_get_tick() - start_time) > timeout_ms) {
                return -1;  /* Timeout */
            }

            /* Read one byte */
            uint8_t ch;
            if (hal_uart_read(UART_ID, &ch, 1, 100) == HAL_OK) {
                /* Check for line ending */
                if (ch == '\r' || ch == '\n') {
                    buffer[index] = '\0';
                    return index;
                }

                /* Check for backspace */
                if (ch == '\b' || ch == 127) {
                    if (index > 0) {
                        index--;
                        uart_printf("\b \b");  /* Erase character on terminal */
                    }
                    continue;
                }

                /* Store character */
                buffer[index++] = ch;

                /* Echo character */
                hal_uart_write(UART_ID, &ch, 1, 100);
            }
        }

        buffer[index] = '\0';
        return index;
    }

    /* Usage */
    int main(void) {
        hal_init();

        hal_uart_config_t config = {
            .baudrate = 115200,
            .wordlen = HAL_UART_WORDLEN_8,
            .stopbits = HAL_UART_STOPBITS_1,
            .parity = HAL_UART_PARITY_NONE,
            .flowctrl = HAL_UART_FLOWCTRL_NONE
        };
        hal_uart_init(UART_ID, &config);

        char line[LINE_BUFFER_SIZE];

        uart_printf("Enter your name: ");

        if (uart_read_line(line, sizeof(line), 30000) > 0) {
            uart_printf("\r\nHello, %s!\r\n", line);
        } else {
            uart_printf("\r\nTimeout!\r\n");
        }

        while (1) {
            hal_delay_ms(1000);
        }

        return 0;
    }

Part 3: Command-Line Interface
------------------------------

Build a simple CLI over UART:

.. code-block:: c

    #include <string.h>
    #include <stdlib.h>

    /**
     * \brief           Parse and execute command
     * \param[in]       cmd: Command string
     */
    static void execute_command(const char* cmd) {
        /* Skip leading whitespace */
        while (*cmd == ' ') cmd++;

        if (strlen(cmd) == 0) {
            return;  /* Empty command */
        }

        /* Help command */
        if (strcmp(cmd, "help") == 0) {
            uart_printf("Available commands:\r\n");
            uart_printf("  help     - Show this help\r\n");
            uart_printf("  led on   - Turn on LED\r\n");
            uart_printf("  led off  - Turn off LED\r\n");
            uart_printf("  tick     - Show system tick\r\n");
            uart_printf("  reset    - Reset system\r\n");
        }
        /* LED commands */
        else if (strncmp(cmd, "led ", 4) == 0) {
            const char* arg = cmd + 4;
            if (strcmp(arg, "on") == 0) {
                hal_gpio_write(HAL_GPIO_PORT_D, 12, HAL_GPIO_LEVEL_HIGH);
                uart_printf("LED ON\r\n");
            } else if (strcmp(arg, "off") == 0) {
                hal_gpio_write(HAL_GPIO_PORT_D, 12, HAL_GPIO_LEVEL_LOW);
                uart_printf("LED OFF\r\n");
            } else {
                uart_printf("Usage: led <on|off>\r\n");
            }
        }
        /* Tick command */
        else if (strcmp(cmd, "tick") == 0) {
            uart_printf("System tick: %lu ms\r\n", (unsigned long)hal_get_tick());
        }
        /* Reset command */
        else if (strcmp(cmd, "reset") == 0) {
            uart_printf("Resetting...\r\n");
            hal_delay_ms(100);
            hal_system_reset();
        }
        /* Unknown command */
        else {
            uart_printf("Unknown command: %s\r\n", cmd);
            uart_printf("Type 'help' for available commands\r\n");
        }
    }

    /**
     * \brief           CLI main loop
     */
    static void cli_loop(void) {
        char line[LINE_BUFFER_SIZE];

        uart_printf("\r\n");
        uart_printf("========================================\r\n");
        uart_printf("  Nexus UART CLI Demo\r\n");
        uart_printf("  Type 'help' for commands\r\n");
        uart_printf("========================================\r\n");

        while (1) {
            uart_printf("\r\nnexus> ");

            if (uart_read_line(line, sizeof(line), 60000) > 0) {
                uart_printf("\r\n");
                execute_command(line);
            }
        }
    }

    int main(void) {
        hal_init();

        /* Initialize LED */
        hal_gpio_config_t led_config = {
            .direction = HAL_GPIO_DIR_OUTPUT,
            .pull = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed = HAL_GPIO_SPEED_LOW,
            .init_level = HAL_GPIO_LEVEL_LOW
        };
        hal_gpio_init(HAL_GPIO_PORT_D, 12, &led_config);

        /* Initialize UART */
        hal_uart_config_t uart_config = {
            .baudrate = 115200,
            .wordlen = HAL_UART_WORDLEN_8,
            .stopbits = HAL_UART_STOPBITS_1,
            .parity = HAL_UART_PARITY_NONE,
            .flowctrl = HAL_UART_FLOWCTRL_NONE
        };
        hal_uart_init(UART_ID, &uart_config);

        /* Run CLI */
        cli_loop();

        return 0;
    }

Part 4: Binary Protocols
------------------------

Implementing Packet-Based Communication
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    /**
     * \brief           Packet structure
     */
    typedef struct {
        uint8_t start_byte;     /* 0xAA */
        uint8_t cmd;            /* Command ID */
        uint8_t length;         /* Payload length */
        uint8_t payload[32];    /* Payload data */
        uint8_t checksum;       /* Simple checksum */
    } __attribute__((packed)) packet_t;

    #define PACKET_START_BYTE  0xAA

    /**
     * \brief           Calculate checksum
     */
    static uint8_t calculate_checksum(const packet_t* pkt) {
        uint8_t sum = 0;
        sum += pkt->cmd;
        sum += pkt->length;
        for (uint8_t i = 0; i < pkt->length; i++) {
            sum += pkt->payload[i];
        }
        return sum;
    }

    /**
     * \brief           Send packet
     */
    static hal_status_t send_packet(uint8_t cmd, const uint8_t* data, uint8_t len) {
        packet_t pkt;

        pkt.start_byte = PACKET_START_BYTE;
        pkt.cmd = cmd;
        pkt.length = len;
        memcpy(pkt.payload, data, len);
        pkt.checksum = calculate_checksum(&pkt);

        size_t packet_size = 4 + len;  /* start + cmd + len + checksum + payload */
        return hal_uart_write(UART_ID, (uint8_t*)&pkt, packet_size, 1000);
    }

    /**
     * \brief           Receive packet
     */
    static hal_status_t receive_packet(packet_t* pkt, uint32_t timeout_ms) {
        uint32_t start_time = hal_get_tick();

        /* Wait for start byte */
        while ((hal_get_tick() - start_time) < timeout_ms) {
            uint8_t byte;
            if (hal_uart_read(UART_ID, &byte, 1, 100) == HAL_OK) {
                if (byte == PACKET_START_BYTE) {
                    pkt->start_byte = byte;
                    break;
                }
            }
        }

        if (pkt->start_byte != PACKET_START_BYTE) {
            return HAL_ERR_TIMEOUT;
        }

        /* Read command and length */
        if (hal_uart_read(UART_ID, &pkt->cmd, 1, 1000) != HAL_OK) {
            return HAL_ERR_TIMEOUT;
        }
        if (hal_uart_read(UART_ID, &pkt->length, 1, 1000) != HAL_OK) {
            return HAL_ERR_TIMEOUT;
        }

        /* Validate length */
        if (pkt->length > sizeof(pkt->payload)) {
            return HAL_ERR_FAIL;
        }

        /* Read payload */
        if (pkt->length > 0) {
            if (hal_uart_read(UART_ID, pkt->payload, pkt->length, 1000) != HAL_OK) {
                return HAL_ERR_TIMEOUT;
            }
        }

        /* Read checksum */
        if (hal_uart_read(UART_ID, &pkt->checksum, 1, 1000) != HAL_OK) {
            return HAL_ERR_TIMEOUT;
        }

        /* Verify checksum */
        uint8_t expected_checksum = calculate_checksum(pkt);
        if (pkt->checksum != expected_checksum) {
            return HAL_ERR_FAIL;  /* Checksum mismatch */
        }

        return HAL_OK;
    }

    /* Usage example */
    int main(void) {
        hal_init();

        hal_uart_config_t config = {
            .baudrate = 115200,
            .wordlen = HAL_UART_WORDLEN_8,
            .stopbits = HAL_UART_STOPBITS_1,
            .parity = HAL_UART_PARITY_NONE,
            .flowctrl = HAL_UART_FLOWCTRL_NONE
        };
        hal_uart_init(UART_ID, &config);

        /* Send a packet */
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        send_packet(0x10, data, sizeof(data));

        /* Receive packets */
        packet_t rx_pkt;
        while (1) {
            if (receive_packet(&rx_pkt, 5000) == HAL_OK) {
                uart_printf("Received packet: cmd=0x%02X, len=%d\r\n",
                           rx_pkt.cmd, rx_pkt.length);
            }
        }

        return 0;
    }

Part 5: Error Handling
----------------------

Handling UART Errors
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    /**
     * \brief           Check and handle UART errors
     */
    static void check_uart_errors(void) {
        hal_uart_error_t errors = hal_uart_get_errors(UART_ID);

        if (errors & HAL_UART_ERROR_OVERRUN) {
            uart_printf("Error: Overrun\r\n");
            /* Data was lost because it wasn't read fast enough */
        }

        if (errors & HAL_UART_ERROR_FRAMING) {
            uart_printf("Error: Framing\r\n");
            /* Invalid stop bit - possible baud rate mismatch */
        }

        if (errors & HAL_UART_ERROR_PARITY) {
            uart_printf("Error: Parity\r\n");
            /* Parity check failed */
        }

        if (errors & HAL_UART_ERROR_NOISE) {
            uart_printf("Error: Noise\r\n");
            /* Noise detected on line */
        }

        /* Clear errors */
        if (errors != HAL_UART_ERROR_NONE) {
            hal_uart_clear_errors(UART_ID);
        }
    }

Part 6: Debugging with UART
---------------------------

Using UART for Debug Output
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    /* Debug macros */
    #define DEBUG_ENABLED  1

    #if DEBUG_ENABLED
    #define DEBUG_PRINT(fmt, ...)  uart_printf("[DEBUG] " fmt, ##__VA_ARGS__)
    #define DEBUG_ERROR(fmt, ...)  uart_printf("[ERROR] " fmt, ##__VA_ARGS__)
    #define DEBUG_INFO(fmt, ...)   uart_printf("[INFO] " fmt, ##__VA_ARGS__)
    #else
    #define DEBUG_PRINT(fmt, ...)
    #define DEBUG_ERROR(fmt, ...)
    #define DEBUG_INFO(fmt, ...)
    #endif

    /* Usage */
    void some_function(void) {
        DEBUG_INFO("Entering function\r\n");

        int result = do_something();

        if (result < 0) {
            DEBUG_ERROR("Operation failed: %d\r\n", result);
        } else {
            DEBUG_PRINT("Result: %d\r\n", result);
        }
    }

Best Practices
--------------

1. **Always Check Return Values**: UART operations can fail or timeout

2. **Use Appropriate Timeouts**: Balance responsiveness vs. reliability

3. **Handle Errors**: Check for and handle UART errors

4. **Buffer Management**: Use appropriate buffer sizes for your application

5. **Flow Control**: Consider hardware flow control for high-speed communication

6. **Baud Rate**: Choose a baud rate supported by both devices

7. **Line Endings**: Use ``\r\n`` for compatibility with most terminals

Common Issues
-------------

**No Output on Terminal:**

- Check wiring (TX/RX crossed correctly)
- Verify baud rate matches on both sides
- Ensure correct UART peripheral is initialized
- Check that terminal is connected to correct COM port

**Garbled Output:**

- Baud rate mismatch
- Wrong data bits, stop bits, or parity settings
- Clock configuration issue

**Data Loss:**

- Increase buffer sizes
- Reduce baud rate
- Implement flow control
- Process received data faster

**Timeout Errors:**

- Increase timeout values
- Check that sender is actually sending data
- Verify wiring and connections

Next Steps
----------

- :doc:`task_creation` - Add multi-tasking with OSAL
- :doc:`../user_guide/log` - Use the Log framework for structured logging
- :doc:`../user_guide/shell` - Use the Shell framework for advanced CLI
- :doc:`../platform_guides/stm32f4` - Platform-specific UART details

