Peripheral Configuration Guide
==============================

This guide provides detailed examples for configuring all peripheral types in the Nexus Embedded Platform using Kconfig.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

The Nexus platform supports comprehensive peripheral configuration through Kconfig. Each peripheral type has:

* **Enable/Disable Control**: Top-level enable for the peripheral type
* **Instance Configuration**: Individual configuration for each peripheral instance
* **Parameter Settings**: Detailed parameters for each instance (baud rate, mode, buffers, etc.)
* **Platform-Specific Options**: Options that vary by platform

Peripheral Types
^^^^^^^^^^^^^^^^

The following peripheral types are supported:

* **UART**: Universal Asynchronous Receiver/Transmitter
* **GPIO**: General Purpose Input/Output
* **SPI**: Serial Peripheral Interface
* **I2C**: Inter-Integrated Circuit
* **Timer**: Hardware timers with PWM support
* **ADC**: Analog-to-Digital Converter
* **CAN**: Controller Area Network (platform-dependent)

Configuration Pattern
^^^^^^^^^^^^^^^^^^^^^

All peripherals follow a consistent configuration pattern:

.. code-block:: Kconfig

    # 1. Enable peripheral type
    config <PLATFORM>_<PERIPHERAL>_ENABLE
        bool "Enable <PERIPHERAL> support"
        default y

    # 2. Enable specific instance
    menuconfig INSTANCE_<PLATFORM>_<PERIPHERAL>_<N>
        bool "Enable <PERIPHERAL><N>"
        default y
        depends on <PLATFORM>_<PERIPHERAL>_ENABLE

    # 3. Configure instance parameters
    if INSTANCE_<PLATFORM>_<PERIPHERAL>_<N>

    config <PERIPHERAL><N>_<PARAMETER>
        <type> "<Parameter description>"
        default <value>

    endif


UART Configuration
------------------

Overview
^^^^^^^^

UART (Universal Asynchronous Receiver/Transmitter) provides serial communication. Configuration includes:

* Baud rate
* Data bits (5-9)
* Stop bits (1-2)
* Parity (None, Odd, Even)
* Transfer mode (Polling, Interrupt, DMA)
* Buffer sizes

Required Parameters
^^^^^^^^^^^^^^^^^^^

**All UART instances require:**

* ``UART<N>_BAUDRATE``: Communication speed in bits per second
* ``UART<N>_DATA_BITS``: Number of data bits per frame
* ``UART<N>_STOP_BITS``: Number of stop bits
* ``UART<N>_PARITY``: Parity checking mode
* ``UART<N>_MODE``: Transfer mode (Polling/Interrupt/DMA)
* ``UART<N>_TX_BUFFER_SIZE``: Transmit buffer size in bytes
* ``UART<N>_RX_BUFFER_SIZE``: Receive buffer size in bytes

Basic UART Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

**Enable UART and configure UART0:**

.. code-block:: Kconfig

    # Enable UART peripheral
    CONFIG_NATIVE_UART_ENABLE=y

    # Enable UART0 instance
    CONFIG_INSTANCE_NX_UART_0=y

    # Configure UART0 parameters
    CONFIG_UART0_BAUDRATE=115200
    CONFIG_UART0_DATA_BITS=8
    CONFIG_UART0_STOP_BITS=1
    CONFIG_NX_UART0_PARITY_NONE=y
    CONFIG_NX_UART0_MODE_INTERRUPT=y
    CONFIG_UART0_TX_BUFFER_SIZE=256
    CONFIG_UART0_RX_BUFFER_SIZE=256

**Generated Configuration:**

.. code-block:: c

    #define NX_CONFIG_NATIVE_UART_ENABLE 1
    #define NX_CONFIG_INSTANCE_NX_UART_0 1
    #define NX_CONFIG_UART0_BAUDRATE 115200
    #define NX_CONFIG_UART0_DATA_BITS 8
    #define NX_CONFIG_UART0_STOP_BITS 1
    #define NX_CONFIG_UART0_PARITY_VALUE 0
    #define NX_CONFIG_UART0_MODE_VALUE 1
    #define NX_CONFIG_UART0_TX_BUFFER_SIZE 256
    #define NX_CONFIG_UART0_RX_BUFFER_SIZE 256

UART Transfer Modes
^^^^^^^^^^^^^^^^^^^

**Polling Mode:**

.. code-block:: Kconfig

    CONFIG_NX_UART0_MODE_POLLING=y

* Simplest implementation
* Blocks CPU during transfers
* No interrupt overhead
* Best for: Simple applications, debugging

**Interrupt Mode:**

.. code-block:: Kconfig

    CONFIG_NX_UART0_MODE_INTERRUPT=y

* Non-blocking transfers
* Uses interrupts for TX/RX
* Moderate CPU overhead
* Best for: General-purpose applications

**DMA Mode:**

.. code-block:: Kconfig

    CONFIG_NX_UART0_MODE_DMA=y

* Most efficient for large transfers
* Minimal CPU involvement
* Requires DMA controller
* Best for: High-throughput applications

UART Parity Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^

**No Parity:**

.. code-block:: Kconfig

    CONFIG_NX_UART0_PARITY_NONE=y

**Odd Parity:**

.. code-block:: Kconfig

    CONFIG_NX_UART0_PARITY_ODD=y

**Even Parity:**

.. code-block:: Kconfig

    CONFIG_NX_UART0_PARITY_EVEN=y

Common UART Configurations
^^^^^^^^^^^^^^^^^^^^^^^^^^

**Debug Console (115200 8N1):**

.. code-block:: Kconfig

    CONFIG_NATIVE_UART_ENABLE=y
    CONFIG_INSTANCE_NX_UART_0=y
    CONFIG_UART0_BAUDRATE=115200
    CONFIG_UART0_DATA_BITS=8
    CONFIG_UART0_STOP_BITS=1
    CONFIG_NX_UART0_PARITY_NONE=y
    CONFIG_NX_UART0_MODE_INTERRUPT=y
    CONFIG_UART0_TX_BUFFER_SIZE=512
    CONFIG_UART0_RX_BUFFER_SIZE=512

**Low-Speed Sensor (9600 8N1):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_UART_1=y
    CONFIG_UART1_BAUDRATE=9600
    CONFIG_UART1_DATA_BITS=8
    CONFIG_UART1_STOP_BITS=1
    CONFIG_NX_UART1_PARITY_NONE=y
    CONFIG_NX_UART1_MODE_POLLING=y
    CONFIG_UART1_TX_BUFFER_SIZE=64
    CONFIG_UART1_RX_BUFFER_SIZE=64

**High-Speed Data (921600 8N1 DMA):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_UART_2=y
    CONFIG_UART2_BAUDRATE=921600
    CONFIG_UART2_DATA_BITS=8
    CONFIG_UART2_STOP_BITS=1
    CONFIG_NX_UART2_PARITY_NONE=y
    CONFIG_NX_UART2_MODE_DMA=y
    CONFIG_UART2_TX_BUFFER_SIZE=2048
    CONFIG_UART2_RX_BUFFER_SIZE=2048

**Industrial Protocol (19200 7E1):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_UART_3=y
    CONFIG_UART3_BAUDRATE=19200
    CONFIG_UART3_DATA_BITS=7
    CONFIG_UART3_STOP_BITS=1
    CONFIG_NX_UART3_PARITY_EVEN=y
    CONFIG_NX_UART3_MODE_INTERRUPT=y
    CONFIG_UART3_TX_BUFFER_SIZE=256
    CONFIG_UART3_RX_BUFFER_SIZE=256

Multiple UART Instances
^^^^^^^^^^^^^^^^^^^^^^^

**Configure multiple UARTs:**

.. code-block:: Kconfig

    # Enable UART peripheral
    CONFIG_NATIVE_UART_ENABLE=y
    CONFIG_NATIVE_UART_MAX_INSTANCES=4

    # UART0: Debug console
    CONFIG_INSTANCE_NX_UART_0=y
    CONFIG_UART0_BAUDRATE=115200
    CONFIG_NX_UART0_MODE_INTERRUPT=y

    # UART1: GPS module
    CONFIG_INSTANCE_NX_UART_1=y
    CONFIG_UART1_BAUDRATE=9600
    CONFIG_NX_UART1_MODE_INTERRUPT=y

    # UART2: Bluetooth module
    CONFIG_INSTANCE_NX_UART_2=y
    CONFIG_UART2_BAUDRATE=115200
    CONFIG_NX_UART2_MODE_DMA=y

    # UART3: RS485 interface
    CONFIG_INSTANCE_NX_UART_3=y
    CONFIG_UART3_BAUDRATE=38400
    CONFIG_NX_UART3_MODE_INTERRUPT=y


GPIO Configuration
------------------

Overview
^^^^^^^^

GPIO (General Purpose Input/Output) provides digital I/O pin control. Configuration includes:

* Pin mode (Input, Output Push-Pull, Output Open-Drain, Alternate Function, Analog)
* Pull configuration (None, Pull-up, Pull-down)
* Speed (Low, Medium, High, Very High)
* Initial output value

Required Parameters
^^^^^^^^^^^^^^^^^^^

**All GPIO pins require:**

* ``GPIO<PORT>_PIN<N>_MODE``: Pin operating mode
* ``GPIO<PORT>_PIN<N>_PULL``: Pull-up/pull-down configuration
* ``GPIO<PORT>_PIN<N>_SPEED``: Pin speed/slew rate
* ``GPIO<PORT>_PIN<N>_OUTPUT_VALUE``: Initial output value (for output modes)

Basic GPIO Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

**Enable GPIO and configure GPIOA PIN0:**

.. code-block:: Kconfig

    # Enable GPIO peripheral
    CONFIG_NATIVE_GPIO_ENABLE=y

    # Enable GPIOA port
    CONFIG_INSTANCE_NX_GPIOA=y

    # Enable and configure GPIOA PIN0
    CONFIG_INSTANCE_NX_GPIOA_PIN0=y
    CONFIG_NX_GPIOA_PIN0_MODE_OUTPUT_PP=y
    CONFIG_NX_GPIOA_PIN0_PULL_NONE=y
    CONFIG_NX_GPIOA_PIN0_SPEED_MEDIUM=y
    CONFIG_GPIOA_PIN0_OUTPUT_VALUE=0

**Generated Configuration:**

.. code-block:: c

    #define NX_CONFIG_NATIVE_GPIO_ENABLE 1
    #define NX_CONFIG_INSTANCE_NX_GPIOA 1
    #define NX_CONFIG_INSTANCE_NX_GPIOA_PIN0 1
    #define NX_CONFIG_GPIOA_PIN0_MODE 1
    #define NX_CONFIG_GPIOA_PIN0_PULL_VALUE 0
    #define NX_CONFIG_GPIOA_PIN0_SPEED_VALUE 1
    #define NX_CONFIG_GPIOA_PIN0_OUTPUT_VALUE 0

GPIO Pin Modes
^^^^^^^^^^^^^^

**Input Mode:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_MODE_INPUT=y
    CONFIG_NX_GPIOA_PIN0_PULL_UP=y  # Optional pull-up

* Read digital input
* High impedance
* Can enable pull-up/pull-down

**Output Push-Pull:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_MODE_OUTPUT_PP=y
    CONFIG_GPIOA_PIN0_OUTPUT_VALUE=0

* Drive high or low
* Strong output
* Most common output mode

**Output Open-Drain:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_MODE_OUTPUT_OD=y
    CONFIG_NX_GPIOA_PIN0_PULL_UP=y  # External pull-up required

* Can only pull low
* Requires external pull-up
* Used for I2C, 1-Wire

**Alternate Function Push-Pull:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_MODE_AF_PP=y

* Controlled by peripheral (UART, SPI, etc.)
* Push-pull output

**Alternate Function Open-Drain:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_MODE_AF_OD=y

* Controlled by peripheral
* Open-drain output

**Analog Mode:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_MODE_ANALOG=y

* Used for ADC/DAC
* Disables digital circuitry

GPIO Pull Configuration
^^^^^^^^^^^^^^^^^^^^^^^

**No Pull:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_PULL_NONE=y

**Pull-Up:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_PULL_UP=y

**Pull-Down:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_PULL_DOWN=y

GPIO Speed Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

**Low Speed:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_SPEED_LOW=y

* Lowest EMI
* Suitable for slow signals

**Medium Speed:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_SPEED_MEDIUM=y

* Balanced performance
* Default for most applications

**High Speed:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_SPEED_HIGH=y

* Fast switching
* Higher EMI

**Very High Speed:**

.. code-block:: Kconfig

    CONFIG_NX_GPIOA_PIN0_SPEED_VERY_HIGH=y

* Fastest switching
* Highest EMI
* For high-speed protocols

Common GPIO Configurations
^^^^^^^^^^^^^^^^^^^^^^^^^^

**LED Output:**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_GPIOA_PIN0=y
    CONFIG_NX_GPIOA_PIN0_MODE_OUTPUT_PP=y
    CONFIG_NX_GPIOA_PIN0_PULL_NONE=y
    CONFIG_NX_GPIOA_PIN0_SPEED_LOW=y
    CONFIG_GPIOA_PIN0_OUTPUT_VALUE=0

**Button Input:**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_GPIOA_PIN1=y
    CONFIG_NX_GPIOA_PIN1_MODE_INPUT=y
    CONFIG_NX_GPIOA_PIN1_PULL_UP=y
    CONFIG_NX_GPIOA_PIN1_SPEED_LOW=y

**I2C SCL/SDA (Open-Drain):**

.. code-block:: Kconfig

    # SCL
    CONFIG_INSTANCE_NX_GPIOA_PIN2=y
    CONFIG_NX_GPIOA_PIN2_MODE_AF_OD=y
    CONFIG_NX_GPIOA_PIN2_PULL_UP=y
    CONFIG_NX_GPIOA_PIN2_SPEED_HIGH=y

    # SDA
    CONFIG_INSTANCE_NX_GPIOB_PIN0=y
    CONFIG_NX_GPIOB_PIN0_MODE_AF_OD=y
    CONFIG_NX_GPIOB_PIN0_PULL_UP=y
    CONFIG_NX_GPIOB_PIN0_SPEED_HIGH=y

**SPI Pins:**

.. code-block:: Kconfig

    # SCK
    CONFIG_INSTANCE_NX_GPIOA_PIN0=y
    CONFIG_NX_GPIOA_PIN0_MODE_AF_PP=y
    CONFIG_NX_GPIOA_PIN0_SPEED_VERY_HIGH=y

    # MOSI
    CONFIG_INSTANCE_NX_GPIOA_PIN1=y
    CONFIG_NX_GPIOA_PIN1_MODE_AF_PP=y
    CONFIG_NX_GPIOA_PIN1_SPEED_VERY_HIGH=y

    # MISO
    CONFIG_INSTANCE_NX_GPIOA_PIN2=y
    CONFIG_NX_GPIOA_PIN2_MODE_INPUT=y
    CONFIG_NX_GPIOA_PIN2_PULL_NONE=y
    CONFIG_NX_GPIOA_PIN2_SPEED_VERY_HIGH=y

Multiple GPIO Ports
^^^^^^^^^^^^^^^^^^^

**Configure multiple GPIO ports:**

.. code-block:: Kconfig

    # Enable GPIO
    CONFIG_NATIVE_GPIO_ENABLE=y

    # GPIOA: LEDs and buttons
    CONFIG_INSTANCE_NX_GPIOA=y
    CONFIG_INSTANCE_NX_GPIOA_PIN0=y  # LED1
    CONFIG_NX_GPIOA_PIN0_MODE_OUTPUT_PP=y
    CONFIG_INSTANCE_NX_GPIOA_PIN1=y  # LED2
    CONFIG_NX_GPIOA_PIN1_MODE_OUTPUT_PP=y
    CONFIG_INSTANCE_NX_GPIOA_PIN2=y  # Button
    CONFIG_NX_GPIOA_PIN2_MODE_INPUT=y
    CONFIG_NX_GPIOA_PIN2_PULL_UP=y

    # GPIOB: I2C interface
    CONFIG_INSTANCE_NX_GPIOB=y
    CONFIG_INSTANCE_NX_GPIOB_PIN0=y  # I2C SCL
    CONFIG_NX_GPIOB_PIN0_MODE_AF_OD=y
    CONFIG_NX_GPIOB_PIN0_PULL_UP=y

    # GPIOC: Analog inputs
    CONFIG_INSTANCE_NX_GPIOC=y
    CONFIG_INSTANCE_NX_GPIOC_PIN0=y  # ADC input
    CONFIG_NX_GPIOC_PIN0_MODE_ANALOG=y


SPI Configuration
-----------------

Overview
^^^^^^^^

SPI (Serial Peripheral Interface) provides high-speed synchronous serial communication. Configuration includes:

* Maximum speed (Hz)
* Clock polarity (CPOL)
* Clock phase (CPHA)
* Bit order (MSB/LSB first)
* Buffer sizes

Required Parameters
^^^^^^^^^^^^^^^^^^^

**All SPI instances require:**

* ``SPI<N>_MAX_SPEED``: Maximum SPI clock speed in Hz
* ``SPI<N>_CPOL``: Clock polarity (0 or 1)
* ``SPI<N>_CPHA``: Clock phase (0 or 1)
* ``SPI<N>_BIT_ORDER``: Bit order (MSB or LSB first)
* ``SPI<N>_TX_BUFFER_SIZE``: Transmit buffer size in bytes
* ``SPI<N>_RX_BUFFER_SIZE``: Receive buffer size in bytes

Basic SPI Configuration
^^^^^^^^^^^^^^^^^^^^^^^

**Enable SPI and configure SPI0:**

.. code-block:: Kconfig

    # Enable SPI peripheral
    CONFIG_NATIVE_SPI_ENABLE=y

    # Enable SPI0 instance
    CONFIG_INSTANCE_NX_SPI_0=y

    # Configure SPI0 parameters
    CONFIG_SPI0_MAX_SPEED=1000000
    CONFIG_NX_SPI0_CPOL_LOW=y
    CONFIG_NX_SPI0_CPHA_EDGE_1=y
    CONFIG_NX_SPI0_MSB_FIRST=y
    CONFIG_SPI0_TX_BUFFER_SIZE=256
    CONFIG_SPI0_RX_BUFFER_SIZE=256

SPI Modes
^^^^^^^^^

SPI has four modes based on CPOL and CPHA:

**Mode 0 (CPOL=0, CPHA=0):**

.. code-block:: Kconfig

    CONFIG_NX_SPI0_CPOL_LOW=y
    CONFIG_NX_SPI0_CPHA_EDGE_1=y

* Clock idle low
* Data sampled on first edge
* Most common mode

**Mode 1 (CPOL=0, CPHA=1):**

.. code-block:: Kconfig

    CONFIG_NX_SPI0_CPOL_LOW=y
    CONFIG_NX_SPI0_CPHA_EDGE_2=y

* Clock idle low
* Data sampled on second edge

**Mode 2 (CPOL=1, CPHA=0):**

.. code-block:: Kconfig

    CONFIG_NX_SPI0_CPOL_HIGH=y
    CONFIG_NX_SPI0_CPHA_EDGE_1=y

* Clock idle high
* Data sampled on first edge

**Mode 3 (CPOL=1, CPHA=1):**

.. code-block:: Kconfig

    CONFIG_NX_SPI0_CPOL_HIGH=y
    CONFIG_NX_SPI0_CPHA_EDGE_2=y

* Clock idle high
* Data sampled on second edge

Common SPI Configurations
^^^^^^^^^^^^^^^^^^^^^^^^^

**SD Card (Mode 0, 25 MHz):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_SPI_0=y
    CONFIG_SPI0_MAX_SPEED=25000000
    CONFIG_NX_SPI0_CPOL_LOW=y
    CONFIG_NX_SPI0_CPHA_EDGE_1=y
    CONFIG_NX_SPI0_MSB_FIRST=y
    CONFIG_SPI0_TX_BUFFER_SIZE=512
    CONFIG_SPI0_RX_BUFFER_SIZE=512

**Flash Memory (Mode 0, 10 MHz):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_SPI_1=y
    CONFIG_SPI1_MAX_SPEED=10000000
    CONFIG_NX_SPI1_CPOL_LOW=y
    CONFIG_NX_SPI1_CPHA_EDGE_1=y
    CONFIG_NX_SPI1_MSB_FIRST=y
    CONFIG_SPI1_TX_BUFFER_SIZE=256
    CONFIG_SPI1_RX_BUFFER_SIZE=256

**Display (Mode 3, 20 MHz):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_SPI_2=y
    CONFIG_SPI2_MAX_SPEED=20000000
    CONFIG_NX_SPI2_CPOL_HIGH=y
    CONFIG_NX_SPI2_CPHA_EDGE_2=y
    CONFIG_NX_SPI2_MSB_FIRST=y
    CONFIG_SPI2_TX_BUFFER_SIZE=1024
    CONFIG_SPI2_RX_BUFFER_SIZE=128

I2C Configuration
-----------------

Overview
^^^^^^^^

I2C (Inter-Integrated Circuit) provides multi-master, multi-slave serial communication. Configuration includes:

* Speed mode (Standard 100kHz, Fast 400kHz, Fast Plus 1MHz)
* Addressing mode (7-bit or 10-bit)
* Buffer sizes

Required Parameters
^^^^^^^^^^^^^^^^^^^

**All I2C instances require:**

* ``I2C<N>_SPEED``: I2C bus speed mode
* ``I2C<N>_ADDR_MODE``: Addressing mode (7-bit or 10-bit)
* ``I2C<N>_TX_BUFFER_SIZE``: Transmit buffer size in bytes
* ``I2C<N>_RX_BUFFER_SIZE``: Receive buffer size in bytes

Basic I2C Configuration
^^^^^^^^^^^^^^^^^^^^^^^

**Enable I2C and configure I2C0:**

.. code-block:: Kconfig

    # Enable I2C peripheral
    CONFIG_NATIVE_I2C_ENABLE=y

    # Enable I2C0 instance
    CONFIG_INSTANCE_NX_I2C_0=y

    # Configure I2C0 parameters
    CONFIG_NX_I2C0_SPEED_FAST=y
    CONFIG_NX_I2C0_ADDR_BIT_7=y
    CONFIG_I2C0_TX_BUFFER_SIZE=256
    CONFIG_I2C0_RX_BUFFER_SIZE=256

I2C Speed Modes
^^^^^^^^^^^^^^^

**Standard Mode (100 kHz):**

.. code-block:: Kconfig

    CONFIG_NX_I2C0_SPEED_STANDARD=y

* 100 kHz bus speed
* Longest cable length
* Best compatibility

**Fast Mode (400 kHz):**

.. code-block:: Kconfig

    CONFIG_NX_I2C0_SPEED_FAST=y

* 400 kHz bus speed
* Most common mode
* Good balance

**Fast Mode Plus (1 MHz):**

.. code-block:: Kconfig

    CONFIG_NX_I2C0_SPEED_FAST_PLUS=y

* 1 MHz bus speed
* Shortest cable length
* Requires capable devices

I2C Addressing Modes
^^^^^^^^^^^^^^^^^^^^

**7-bit Addressing:**

.. code-block:: Kconfig

    CONFIG_NX_I2C0_ADDR_BIT_7=y

* Standard addressing
* 128 possible addresses
* Most common

**10-bit Addressing:**

.. code-block:: Kconfig

    CONFIG_NX_I2C0_ADDR_BIT_10=y

* Extended addressing
* 1024 possible addresses
* Less common

Common I2C Configurations
^^^^^^^^^^^^^^^^^^^^^^^^^

**Sensor Bus (Fast Mode, 7-bit):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_I2C_0=y
    CONFIG_NX_I2C0_SPEED_FAST=y
    CONFIG_NX_I2C0_ADDR_BIT_7=y
    CONFIG_I2C0_TX_BUFFER_SIZE=128
    CONFIG_I2C0_RX_BUFFER_SIZE=128

**EEPROM (Standard Mode, 7-bit):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_I2C_1=y
    CONFIG_NX_I2C1_SPEED_STANDARD=y
    CONFIG_NX_I2C1_ADDR_BIT_7=y
    CONFIG_I2C1_TX_BUFFER_SIZE=256
    CONFIG_I2C1_RX_BUFFER_SIZE=256

**High-Speed Sensors (Fast Plus, 7-bit):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_I2C_2=y
    CONFIG_NX_I2C2_SPEED_FAST_PLUS=y
    CONFIG_NX_I2C2_ADDR_BIT_7=y
    CONFIG_I2C2_TX_BUFFER_SIZE=512
    CONFIG_I2C2_RX_BUFFER_SIZE=512

Timer Configuration
-------------------

Overview
^^^^^^^^

Timer peripherals provide timing, counting, and PWM generation. Configuration includes:

* Timer frequency (Hz)
* Counter mode (Up, Down, Center-aligned)
* PWM channel count

Required Parameters
^^^^^^^^^^^^^^^^^^^

**All Timer instances require:**

* ``TIMER<N>_FREQUENCY``: Timer base frequency in Hz
* ``TIMER<N>_MODE``: Counter mode
* ``TIMER<N>_CHANNEL_COUNT``: Number of PWM channels

Basic Timer Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^

**Enable Timer and configure TIMER0:**

.. code-block:: Kconfig

    # Enable Timer peripheral
    CONFIG_NATIVE_TIMER_ENABLE=y

    # Enable TIMER0 instance
    CONFIG_INSTANCE_NX_TIMER_0=y

    # Configure TIMER0 parameters
    CONFIG_TIMER0_FREQUENCY=1000000
    CONFIG_NX_TIMER0_MODE_UP=y
    CONFIG_TIMER0_CHANNEL_COUNT=4

Timer Counter Modes
^^^^^^^^^^^^^^^^^^^

**Up Counting:**

.. code-block:: Kconfig

    CONFIG_NX_TIMER0_MODE_UP=y

* Counts from 0 to period
* Most common mode

**Down Counting:**

.. code-block:: Kconfig

    CONFIG_NX_TIMER0_MODE_DOWN=y

* Counts from period to 0

**Center-Aligned:**

.. code-block:: Kconfig

    CONFIG_NX_TIMER0_MODE_CENTER=y

* Counts up then down
* Used for symmetric PWM

Common Timer Configurations
^^^^^^^^^^^^^^^^^^^^^^^^^^^

**PWM Generation (1 MHz, 4 channels):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_TIMER_0=y
    CONFIG_TIMER0_FREQUENCY=1000000
    CONFIG_NX_TIMER0_MODE_UP=y
    CONFIG_TIMER0_CHANNEL_COUNT=4

**Periodic Interrupt (10 kHz):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_TIMER_1=y
    CONFIG_TIMER1_FREQUENCY=10000
    CONFIG_NX_TIMER1_MODE_UP=y
    CONFIG_TIMER1_CHANNEL_COUNT=0

**Motor Control (20 kHz, Center-aligned):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_TIMER_2=y
    CONFIG_TIMER2_FREQUENCY=20000
    CONFIG_NX_TIMER2_MODE_CENTER=y
    CONFIG_TIMER2_CHANNEL_COUNT=6

ADC Configuration
-----------------

Overview
^^^^^^^^

ADC (Analog-to-Digital Converter) converts analog signals to digital values. Configuration includes:

* Resolution (8, 10, 12, 16 bits)
* Sampling time
* Channel count

Required Parameters
^^^^^^^^^^^^^^^^^^^

**All ADC instances require:**

* ``ADC<N>_RESOLUTION``: ADC resolution in bits
* ``ADC<N>_SAMPLE_TIME``: Sampling time
* ``ADC<N>_CHANNEL_COUNT``: Number of ADC channels

Basic ADC Configuration
^^^^^^^^^^^^^^^^^^^^^^^

**Enable ADC and configure ADC0:**

.. code-block:: Kconfig

    # Enable ADC peripheral
    CONFIG_NATIVE_ADC_ENABLE=y

    # Enable ADC0 instance
    CONFIG_INSTANCE_NX_ADC_0=y

    # Configure ADC0 parameters
    CONFIG_NX_ADC0_RESOLUTION_12=y
    CONFIG_NX_ADC0_SAMPLE_TIME_MEDIUM=y
    CONFIG_ADC0_CHANNEL_COUNT=16

ADC Resolution
^^^^^^^^^^^^^^

**8-bit Resolution:**

.. code-block:: Kconfig

    CONFIG_NX_ADC0_RESOLUTION_8=y

* 256 levels (0-255)
* Fastest conversion
* Lowest precision

**10-bit Resolution:**

.. code-block:: Kconfig

    CONFIG_NX_ADC0_RESOLUTION_10=y

* 1024 levels (0-1023)
* Good balance

**12-bit Resolution:**

.. code-block:: Kconfig

    CONFIG_NX_ADC0_RESOLUTION_12=y

* 4096 levels (0-4095)
* Most common
* Good precision

**16-bit Resolution:**

.. code-block:: Kconfig

    CONFIG_NX_ADC0_RESOLUTION_16=y

* 65536 levels (0-65535)
* Highest precision
* Slowest conversion

ADC Sampling Time
^^^^^^^^^^^^^^^^^

**Fast Sampling:**

.. code-block:: Kconfig

    CONFIG_NX_ADC0_SAMPLE_TIME_FAST=y

* Shortest sampling time
* Requires low source impedance

**Medium Sampling:**

.. code-block:: Kconfig

    CONFIG_NX_ADC0_SAMPLE_TIME_MEDIUM=y

* Balanced sampling time
* Most common

**Slow Sampling:**

.. code-block:: Kconfig

    CONFIG_NX_ADC0_SAMPLE_TIME_SLOW=y

* Longest sampling time
* Best for high impedance sources

Common ADC Configurations
^^^^^^^^^^^^^^^^^^^^^^^^^

**General Purpose (12-bit, Medium):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_ADC_0=y
    CONFIG_NX_ADC0_RESOLUTION_12=y
    CONFIG_NX_ADC0_SAMPLE_TIME_MEDIUM=y
    CONFIG_ADC0_CHANNEL_COUNT=8

**High-Speed (8-bit, Fast):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_ADC_1=y
    CONFIG_NX_ADC1_RESOLUTION_BIT_8=y
    CONFIG_NX_ADC1_SAMPLE_TIME_FAST=y
    CONFIG_ADC1_CHANNEL_COUNT=4

**High-Precision (16-bit, Slow):**

.. code-block:: Kconfig

    CONFIG_INSTANCE_NX_ADC_2=y
    CONFIG_NX_ADC2_RESOLUTION_BIT_16=y
    CONFIG_NX_ADC2_SAMPLE_TIME_SLOW=y
    CONFIG_ADC2_CHANNEL_COUNT=4

ADC Buffer Configuration
^^^^^^^^^^^^^^^^^^^^^^^^

**Enable ADC Buffer for multi-channel sampling:**

.. code-block:: Kconfig

    # Enable ADC Buffer
    CONFIG_NATIVE_ADC_BUFFER_ENABLE=y

    # Configure ADC_BUFFER0
    CONFIG_INSTANCE_NX_ADC_BUFFER_0=y
    CONFIG_ADC_BUFFER0_CHANNEL_COUNT=4
    CONFIG_ADC_BUFFER0_BUFFER_SIZE=256
    CONFIG_NX_ADC_BUFFER0_TRIGGER_TIMER=y

See Also
--------

* :doc:`kconfig_tutorial` - Kconfig tutorial
* :doc:`kconfig_platforms` - Platform-specific configuration
* :doc:`kconfig_tools` - Configuration tools reference
* :doc:`../api/hal` - HAL API reference
