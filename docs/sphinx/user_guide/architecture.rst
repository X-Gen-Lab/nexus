Architecture
============

This document provides a detailed overview of the Nexus platform architecture.

Design Principles
-----------------

Nexus is built on the following design principles:

1. **Separation of Concerns**: Each layer has a specific responsibility
2. **Abstraction**: Hide implementation details behind clean interfaces
3. **Portability**: Minimize platform-specific code in applications
4. **Testability**: Design for unit testing and simulation
5. **Resource Efficiency**: Optimize for constrained embedded systems

System Overview
---------------

The following diagram provides a high-level overview of the complete Nexus system architecture:

.. mermaid::
   :alt: Complete Nexus system architecture showing all layers, components, and their relationships

   graph TB
       subgraph "Application Layer"
           APP[User Applications]
       end

       subgraph "Framework Layer"
           direction LR
           SHELL[Shell CLI]
           LOG[Logging]
           CONFIG[Configuration]
           INIT[Initialization]
       end

       subgraph "OSAL - OS Abstraction"
           direction LR
           TASK[Tasks/Threads]
           SYNC[Mutex/Semaphore]
           QUEUE[Message Queues]
           TIMER[Software Timers]
           EVENT[Event Flags]
       end

       subgraph "HAL - Hardware Abstraction"
           direction LR
           GPIO[GPIO]
           UART[UART]
           SPI[SPI]
           I2C[I2C]
           TIMER_HW[Timer/PWM]
           ADC[ADC]
           CAN[CAN]
       end

       subgraph "Platform Adapters"
           direction LR
           FREERTOS[FreeRTOS]
           NATIVE[Native/POSIX]
           BAREMETAL[Bare Metal]
       end

       subgraph "Platform Implementations"
           direction LR
           STM32F4[STM32F4xx]
           STM32H7[STM32H7xx]
           GD32[GD32VF103]
           NATIVE_PLAT[Native/PC]
       end

       subgraph "Hardware"
           HW[MCU Hardware]
       end

       APP --> SHELL
       APP --> LOG
       APP --> CONFIG
       APP --> INIT
       APP --> TASK
       APP --> GPIO

       SHELL --> LOG
       SHELL --> TASK
       CONFIG --> TASK
       INIT --> TASK

       TASK --> FREERTOS
       TASK --> NATIVE
       TASK --> BAREMETAL
       SYNC --> FREERTOS
       SYNC --> NATIVE
       QUEUE --> FREERTOS
       TIMER --> FREERTOS
       EVENT --> FREERTOS

       GPIO --> STM32F4
       GPIO --> STM32H7
       GPIO --> GD32
       GPIO --> NATIVE_PLAT
       UART --> STM32F4
       UART --> STM32H7
       SPI --> STM32F4
       I2C --> STM32F4
       TIMER_HW --> STM32F4
       ADC --> STM32F4
       CAN --> STM32F4

       STM32F4 --> HW
       STM32H7 --> HW
       GD32 --> HW

       style APP fill:#e1f5ff
       style SHELL fill:#fff4e1
       style LOG fill:#fff4e1
       style CONFIG fill:#fff4e1
       style INIT fill:#fff4e1
       style TASK fill:#ffe1f5
       style SYNC fill:#ffe1f5
       style QUEUE fill:#ffe1f5
       style TIMER fill:#ffe1f5
       style EVENT fill:#ffe1f5
       style GPIO fill:#e1ffe1
       style UART fill:#e1ffe1
       style SPI fill:#e1ffe1
       style I2C fill:#e1ffe1
       style TIMER_HW fill:#e1ffe1
       style ADC fill:#e1ffe1
       style CAN fill:#e1ffe1
       style FREERTOS fill:#f5e1ff
       style NATIVE fill:#f5e1ff
       style BAREMETAL fill:#f5e1ff
       style STM32F4 fill:#ffe1e1
       style STM32H7 fill:#ffe1e1
       style GD32 fill:#ffe1e1
       style NATIVE_PLAT fill:#ffe1e1
       style HW fill:#cccccc

Layer Architecture
------------------

The Nexus platform follows a layered architecture with clear separation of concerns:

.. mermaid::
   :alt: Nexus Platform Layer Architecture showing Applications, Middleware, OSAL, HAL, and Platform layers

   graph TB
       subgraph Applications["Application Layer"]
           APP1[User Application 1]
           APP2[User Application 2]
           APP3[User Application N]
       end

       subgraph Middleware["Middleware Layer"]
           SHELL[Shell Framework]
           LOG[Log Framework]
           CONFIG[Config Manager]
           INIT[Init Framework]
       end

       subgraph OSAL["OS Abstraction Layer"]
           TASK[Task Management]
           MUTEX[Mutex/Semaphore]
           QUEUE[Message Queue]
           TIMER[Software Timer]
       end

       subgraph HAL["Hardware Abstraction Layer"]
           GPIO[GPIO Driver]
           UART[UART Driver]
           SPI[SPI Driver]
           I2C[I2C Driver]
           ADC[ADC Driver]
       end

       subgraph Platform["Platform Layer"]
           STM32F4[STM32F4 Platform]
           STM32H7[STM32H7 Platform]
           NATIVE[Native Platform]
           GD32[GD32 Platform]
       end

       Applications --> Middleware
       Applications --> OSAL
       Applications --> HAL
       Middleware --> OSAL
       Middleware --> HAL
       OSAL --> HAL
       HAL --> Platform

       style Applications fill:#e1f5ff
       style Middleware fill:#fff4e1
       style OSAL fill:#ffe1f5
       style HAL fill:#e1ffe1
       style Platform fill:#f5e1ff

Application Layer
~~~~~~~~~~~~~~~~~

User applications built on top of Nexus platform. Applications should:

- Use only public APIs from lower layers
- Avoid direct hardware access
- Be portable across supported platforms

Middleware Layer
~~~~~~~~~~~~~~~~

Common services that provide higher-level functionality:

- **Log Framework**: Flexible logging with multiple backends
- **Shell**: Command-line interface for debugging
- **Config**: Configuration management and persistence
- **Event**: Event-driven programming support

See :doc:`log` for detailed Log Framework documentation.

OSAL Layer
~~~~~~~~~~

OS Abstraction Layer provides portable RTOS primitives:

- **Task**: Thread/task management
- **Mutex**: Mutual exclusion locks
- **Semaphore**: Binary and counting semaphores
- **Queue**: Message queues for inter-task communication
- **Timer**: Software timers

Supported backends:

- FreeRTOS
- Native (pthreads for PC simulation)
- Bare-metal (cooperative scheduling)

See :doc:`osal` for detailed OSAL documentation.

HAL Layer
~~~~~~~~~

Hardware Abstraction Layer provides unified peripheral APIs:

- **GPIO**: General-purpose I/O
- **UART**: Serial communication
- **SPI**: SPI bus master/slave
- **I2C**: I2C bus master/slave
- **Timer**: Hardware timers and PWM
- **ADC**: Analog-to-digital conversion

See :doc:`hal` for detailed HAL documentation.

Platform Layer
~~~~~~~~~~~~~~

MCU-specific implementations including:

- Startup code and system initialization
- Linker scripts
- Vendor SDK integration
- Clock and power management

Data Flow
---------

The following diagram illustrates how data flows through the Nexus architecture:

.. mermaid::
   :alt: Data flow diagram showing how requests flow from Application through Middleware, OSAL, HAL to Platform

   sequenceDiagram
       participant App as Application
       participant MW as Middleware
       participant OSAL as OSAL
       participant HAL as HAL
       participant Platform as Platform

       App->>MW: Request Service
       MW->>OSAL: Create Task
       OSAL->>HAL: Configure Timer
       HAL->>Platform: Access Hardware
       Platform-->>HAL: Hardware Response
       HAL-->>OSAL: Timer Configured
       OSAL-->>MW: Task Created
       MW-->>App: Service Ready

       App->>HAL: GPIO Write
       HAL->>Platform: Set Pin State
       Platform-->>HAL: Pin Set
       HAL-->>App: Write Complete

Dependency Rules
----------------

Strict dependency rules ensure clean architecture:

1. **Upper layers depend on lower layers** (never reverse)
2. **Same-layer components are independent** (no horizontal dependencies)
3. **Platform layer is the only layer with hardware access**

.. mermaid::
   :alt: Dependency rules diagram showing allowed and forbidden dependencies between layers

   graph LR
       subgraph "Allowed Dependencies"
           A1[Application] --> M1[Middleware]
           M1 --> O1[OSAL]
           O1 --> H1[HAL]
           H1 --> P1[Platform]
       end

       subgraph "Forbidden Dependencies"
           H2[HAL] -.X.- M2[Middleware]
           G[GPIO] -.X.- U[UART]
       end

       style A1 fill:#e1f5ff
       style M1 fill:#fff4e1
       style O1 fill:#ffe1f5
       style H1 fill:#e1ffe1
       style P1 fill:#f5e1ff
       style H2 fill:#ffcccc
       style M2 fill:#ffcccc
       style G fill:#ffcccc
       style U fill:#ffcccc

Memory Management
-----------------

Nexus supports multiple memory management strategies:

**Static Allocation**
    All memory allocated at compile time. Suitable for safety-critical systems.

**Dynamic Allocation**
    Runtime memory allocation using malloc/free. More flexible but requires
    careful management.

**Pool Allocation**
    Fixed-size memory pools for predictable allocation patterns.

.. mermaid::
   :alt: Memory management strategies showing static, dynamic, and pool allocation approaches

   graph TB
       subgraph "Static Allocation"
           S1[Compile Time]
           S2[Fixed Arrays]
           S3[No Runtime Overhead]
           S1 --> S2 --> S3
       end

       subgraph "Dynamic Allocation"
           D1[Runtime malloc/free]
           D2[Flexible Size]
           D3[Fragmentation Risk]
           D1 --> D2 --> D3
       end

       subgraph "Pool Allocation"
           P1[Fixed-Size Blocks]
           P2[Fast Allocation]
           P3[No Fragmentation]
           P1 --> P2 --> P3
       end

       APP[Application] --> S1
       APP --> D1
       APP --> P1

       style S1 fill:#e1ffe1
       style S2 fill:#e1ffe1
       style S3 fill:#e1ffe1
       style D1 fill:#fff4e1
       style D2 fill:#fff4e1
       style D3 fill:#fff4e1
       style P1 fill:#e1f5ff
       style P2 fill:#e1f5ff
       style P3 fill:#e1f5ff

Configuration example:

.. code-block:: c

    /* Static allocation mode */
    #define NEXUS_USE_STATIC_ALLOC  1
    #define NEXUS_MAX_TASKS         8
    #define NEXUS_MAX_MUTEXES       16

Error Handling
--------------

All Nexus APIs follow consistent error handling:

.. code-block:: c

    /* All functions return status codes */
    hal_status_t status = hal_gpio_init(port, pin, &config);
    if (status != HAL_OK) {
        /* Handle error */
        LOG_ERROR("GPIO init failed: %d", status);
    }

Common status codes:

+------------------------+----------------------------------+
| Status                 | Description                      |
+========================+==================================+
| ``XXX_OK``             | Operation successful             |
+------------------------+----------------------------------+
| ``XXX_ERROR``          | General error                    |
+------------------------+----------------------------------+
| ``XXX_ERROR_PARAM``    | Invalid parameter                |
+------------------------+----------------------------------+
| ``XXX_ERROR_STATE``    | Invalid state                    |
+------------------------+----------------------------------+
| ``XXX_ERROR_TIMEOUT``  | Operation timed out              |
+------------------------+----------------------------------+
| ``XXX_ERROR_NO_MEMORY``| Memory allocation failed         |
+------------------------+----------------------------------+

Build System
------------

Nexus uses CMake for cross-platform build configuration:

.. code-block:: bash

    # Select platform
    CMake -B build -DNEXUS_PLATFORM=stm32f4

    # Enable features
    CMake -B build \
        -DNEXUS_PLATFORM=stm32f4 \
        -DNEXUS_BUILD_TESTS=ON \
        -DNEXUS_ENABLE_LOG=ON

Key CMake options:

+---------------------------+----------------------------------+
| Option                    | Description                      |
+===========================+==================================+
| ``NEXUS_PLATFORM``        | Target platform (stm32f4, native)|
+---------------------------+----------------------------------+
| ``NEXUS_BUILD_TESTS``     | Build unit tests                 |
+---------------------------+----------------------------------+
| ``NEXUS_ENABLE_LOG``      | Enable log framework             |
+---------------------------+----------------------------------+
| ``NEXUS_ENABLE_COVERAGE`` | Enable code coverage             |
+---------------------------+----------------------------------+

Next Steps
----------

- :doc:`hal` - Learn about the Hardware Abstraction Layer
- :doc:`osal` - Learn about the OS Abstraction Layer
- :doc:`log` - Learn about the Log Framework
- :doc:`porting` - Port Nexus to a new platform
