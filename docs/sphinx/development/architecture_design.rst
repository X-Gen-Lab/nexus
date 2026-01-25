Architecture Design
===================

This document describes the architecture and design decisions of the Nexus Embedded Platform.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

Nexus follows a layered architecture with clear separation of concerns. This document explains:

* System architecture
* Design principles
* Design patterns
* Module interactions
* Design decisions and rationale

Design Principles
-----------------

Core Principles
~~~~~~~~~~~~~~~

**1. Separation of Concerns**

Each layer has a specific responsibility:

* **HAL**: Hardware abstraction
* **OSAL**: OS abstraction
* **Framework**: High-level services
* **Platform**: Hardware-specific implementation

**2. Abstraction**

Hide implementation details behind clean interfaces:

* Public APIs are stable and well-documented
* Internal implementation can change without affecting users
* Platform differences are hidden from applications

**3. Portability**

Minimize platform-specific code:

* Write once, run on multiple platforms
* Platform-specific code isolated in platform layer
* Consistent behavior across platforms

**4. Testability**

Design for testing:

* Native platform for unit testing
* Mock-friendly interfaces
* Dependency injection
* Property-based testing support

**5. Resource Efficiency**

Optimize for embedded systems:

* Minimal memory footprint
* Low CPU overhead
* Configurable features via Kconfig
* Static allocation options

SOLID Principles
~~~~~~~~~~~~~~~~

**Single Responsibility Principle**

Each module has one reason to change:

* HAL modules handle only hardware abstraction
* OSAL modules handle only OS abstraction
* Framework modules provide specific services

**Open/Closed Principle**

Open for extension, closed for modification:

* New platforms can be added without modifying core
* New peripherals can be added via factory pattern
* Behavior can be extended via configuration

**Liskov Substitution Principle**

Derived types must be substitutable:

* All GPIO implementations follow same interface
* All UART implementations behave consistently
* Platform-specific code respects contracts

**Interface Segregation Principle**

Clients shouldn't depend on unused interfaces:

* Separate read/write GPIO interfaces
* Separate TX/RX UART interfaces
* Optional features are truly optional

**Dependency Inversion Principle**

Depend on abstractions, not concretions:

* Applications depend on HAL interfaces
* HAL depends on platform abstractions
* Platform implements interfaces

System Architecture
-------------------

Layer Overview
~~~~~~~~~~~~~~

.. code-block:: text

   ┌─────────────────────────────────────────────────────────────┐
   │                      Application Layer                       │
   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
   │  │   Blinky     │  │  Shell Demo  │  │ Config Demo  │      │
   │  └──────────────┘  └──────────────┘  └──────────────┘      │
   ├─────────────────────────────────────────────────────────────┤
   │                      Framework Layer                         │
   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
   │  │     Log      │  │    Shell     │  │    Config    │      │
   │  └──────────────┘  └──────────────┘  └──────────────┘      │
   ├─────────────────────────────────────────────────────────────┤
   │                         OSAL Layer                           │
   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
   │  │     Task     │  │    Mutex     │  │    Queue     │      │
   │  └──────────────┘  └──────────────┘  └──────────────┘      │
   ├─────────────────────────────────────────────────────────────┤
   │                          HAL Layer                           │
   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
   │  │     GPIO     │  │     UART     │  │     SPI      │      │
   │  └──────────────┘  └──────────────┘  └──────────────┘      │
   ├─────────────────────────────────────────────────────────────┤
   │                       Platform Layer                         │
   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
   │  │   STM32F4    │  │   STM32H7    │  │    Native    │      │
   │  └──────────────┘  └──────────────┘  └──────────────┘      │
   └─────────────────────────────────────────────────────────────┘

Dependency Rules
~~~~~~~~~~~~~~~~

**Strict Layering**:

1. Upper layers depend on lower layers
2. Lower layers never depend on upper layers
3. Same-layer components are independent
4. Platform layer is the only layer with hardware access

**Allowed Dependencies**:

.. code-block:: text

   Application → Framework → OSAL → HAL → Platform
   Application → OSAL → HAL → Platform
   Application → HAL → Platform

**Forbidden Dependencies**:

.. code-block:: text

   HAL ✗→ Framework
   Platform ✗→ HAL
   GPIO ✗→ UART (same layer)

HAL Architecture
----------------

Design Goals
~~~~~~~~~~~~

* Unified API across platforms
* Type-safe interfaces
* Resource management
* Error handling
* Testability

Factory Pattern
~~~~~~~~~~~~~~~

HAL uses factory pattern for device creation:

**Benefits**:

* Centralized resource management
* Platform-specific instantiation
* Lifecycle management
* Resource tracking

**Implementation**:

.. code-block:: c

   /* Factory function */
   nx_gpio_write_t* nx_factory_gpio_write(char port, uint8_t pin) {
       /* 1. Validate parameters */
       if (!is_valid_port(port) || !is_valid_pin(pin)) {
           return NULL;
       }

       /* 2. Check resource availability */
       if (!is_gpio_available(port, pin)) {
           return NULL;
       }

       /* 3. Create platform-specific instance */
       nx_gpio_write_t* gpio = platform_create_gpio_write(port, pin);
       if (gpio == NULL) {
           return NULL;
       }

       /* 4. Register resource */
       register_gpio_resource(port, pin, gpio);

       return gpio;
   }

   /* Release function */
   void nx_factory_gpio_release(nx_gpio_t* gpio) {
       if (gpio == NULL) {
           return;
       }

       /* 1. Unregister resource */
       unregister_gpio_resource(gpio);

       /* 2. Deinitialize device */
       if (gpio->deinit != NULL) {
           gpio->deinit(gpio);
       }

       /* 3. Free memory */
       platform_free_gpio(gpio);
   }

Interface Pattern
~~~~~~~~~~~~~~~~~

Devices expose interfaces with function pointers:

**Benefits**:

* Polymorphism in C
* Platform-specific implementations
* Runtime dispatch
* Testability (mocking)

**Implementation**:

.. code-block:: c

   /* Interface definition */
   typedef struct nx_gpio_write {
       /* Base interface */
       nx_gpio_t base;

       /* Write operations */
       nx_status_t (*write)(struct nx_gpio_write* self, uint8_t pin,
                           nx_gpio_level_t level);
       nx_status_t (*toggle)(struct nx_gpio_write* self, uint8_t pin);
       nx_status_t (*read)(struct nx_gpio_write* self, uint8_t pin,
                          nx_gpio_level_t* level);

       /* Configuration */
       nx_status_t (*set_mode)(struct nx_gpio_write* self, uint8_t pin,
                              nx_gpio_mode_t mode);

       /* Private data */
       void* private_data;
   } nx_gpio_write_t;

   /* Platform implementation */
   static nx_status_t stm32_gpio_write(nx_gpio_write_t* self, uint8_t pin,
                                      nx_gpio_level_t level) {
       stm32_gpio_private_t* priv = (stm32_gpio_private_t*)self->private_data;

       /* Platform-specific implementation */
       HAL_GPIO_WritePin(priv->port, 1 << pin,
                        level == NX_GPIO_LEVEL_HIGH ? GPIO_PIN_SET : GPIO_PIN_RESET);

       return NX_OK;
   }

Resource Management
~~~~~~~~~~~~~~~~~~~

HAL manages hardware resources:

**Resource Registry**:

.. code-block:: c

   /* Resource entry */
   typedef struct {
       uint8_t port;
       uint8_t pin;
       nx_gpio_t* device;
       bool in_use;
   } gpio_resource_t;

   /* Resource registry */
   static gpio_resource_t gpio_resources[MAX_GPIO_RESOURCES];

   /* Register resource */
   static nx_status_t register_gpio_resource(uint8_t port, uint8_t pin,
                                            nx_gpio_t* device) {
       for (int i = 0; i < MAX_GPIO_RESOURCES; i++) {
           if (!gpio_resources[i].in_use) {
               gpio_resources[i].port = port;
               gpio_resources[i].pin = pin;
               gpio_resources[i].device = device;
               gpio_resources[i].in_use = true;
               return NX_OK;
           }
       }
       return NX_ERR_NO_MEMORY;
   }

   /* Check availability */
   static bool is_gpio_available(uint8_t port, uint8_t pin) {
       for (int i = 0; i < MAX_GPIO_RESOURCES; i++) {
           if (gpio_resources[i].in_use &&
               gpio_resources[i].port == port &&
               gpio_resources[i].pin == pin) {
               return false;
           }
       }
       return true;
   }

Error Handling
~~~~~~~~~~~~~~

Consistent error handling across HAL:

**Status Codes**:

.. code-block:: c

   typedef enum {
       NX_OK = 0,              /* Success */
       NX_ERR_FAIL,            /* General failure */
       NX_ERR_PARAM,           /* Invalid parameter */
       NX_ERR_STATE,           /* Invalid state */
       NX_ERR_TIMEOUT,         /* Operation timeout */
       NX_ERR_NO_MEMORY,       /* Out of memory */
       NX_ERR_NOT_SUPPORTED,   /* Not supported */
       NX_ERR_BUSY,            /* Resource busy */
   } nx_status_t;

**Error Propagation**:

.. code-block:: c

   nx_status_t hal_operation(void) {
       nx_status_t status;

       /* Check preconditions */
       if (!is_initialized()) {
           return NX_ERR_STATE;
       }

       /* Perform operation */
       status = platform_operation();
       if (status != NX_OK) {
           /* Log error */
           LOG_ERROR("Operation failed: %d", status);
           return status;
       }

       /* Verify postconditions */
       if (!verify_result()) {
           return NX_ERR_FAIL;
       }

       return NX_OK;
   }

OSAL Architecture
-----------------

Design Goals
~~~~~~~~~~~~

* OS-agnostic API
* Multiple backend support
* Minimal overhead
* Consistent behavior

Adapter Pattern
~~~~~~~~~~~~~~~

OSAL uses adapter pattern for OS abstraction:

**Benefits**:

* Support multiple RTOS
* Consistent API
* Easy to add new backends
* Testability

**Implementation**:

.. code-block:: c

   /* OSAL API */
   osal_status_t osal_mutex_create(osal_mutex_handle_t* handle);

   /* FreeRTOS adapter */
   osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
       SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
       if (mutex == NULL) {
           return OSAL_ERR_NO_MEMORY;
       }
       *handle = (osal_mutex_handle_t)mutex;
       return OSAL_OK;
   }

   /* Bare-metal adapter */
   osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
       baremetal_mutex_t* mutex = malloc(sizeof(baremetal_mutex_t));
       if (mutex == NULL) {
           return OSAL_ERR_NO_MEMORY;
       }
       mutex->locked = false;
       *handle = (osal_mutex_handle_t)mutex;
       return OSAL_OK;
   }

Backend Selection
~~~~~~~~~~~~~~~~~

Backend selected at compile time via Kconfig:

.. code-block:: c

   #if defined(CONFIG_OSAL_FREERTOS)
   #include "osal/adapters/freertos/freertos_adapter.h"
   #elif defined(CONFIG_OSAL_BAREMETAL)
   #include "osal/adapters/baremetal/baremetal_adapter.h"
   #elif defined(CONFIG_OSAL_RTTHREAD)
   #include "osal/adapters/rtthread/rtthread_adapter.h"
   #else
   #error "No OSAL backend selected"
   #endif

Framework Architecture
----------------------

Design Goals
~~~~~~~~~~~~

* Reusable components
* Minimal dependencies
* Configurable features
* Easy integration

Logging Framework
~~~~~~~~~~~~~~~~~

**Architecture**:

.. code-block:: text

   ┌─────────────────────────────────────────┐
   │          Application Code               │
   │  LOG_INFO("Message: %d", value)         │
   └─────────────────┬───────────────────────┘
                     │
   ┌─────────────────▼───────────────────────┐
   │          Log Frontend                   │
   │  - Level filtering                      │
   │  - Module filtering                     │
   │  - Format string processing             │
   └─────────────────┬───────────────────────┘
                     │
   ┌─────────────────▼───────────────────────┐
   │          Log Backend                    │
   │  ┌──────────┐  ┌──────────┐            │
   │  │ Console  │  │   UART   │            │
   │  └──────────┘  └──────────┘            │
   └─────────────────────────────────────────┘

**Key Features**:

* Multiple log levels (ERROR, WARN, INFO, DEBUG)
* Module-based filtering
* Multiple backends (console, UART, file)
* Asynchronous logging option
* Minimal overhead when disabled

Shell Framework
~~~~~~~~~~~~~~~

**Architecture**:

.. code-block:: text

   ┌─────────────────────────────────────────┐
   │          User Input                     │
   │  "led green on"                         │
   └─────────────────┬───────────────────────┘
                     │
   ┌─────────────────▼───────────────────────┐
   │          Shell Core                     │
   │  - Command parsing                      │
   │  - History management                   │
   │  - Tab completion                       │
   └─────────────────┬───────────────────────┘
                     │
   ┌─────────────────▼───────────────────────┐
   │       Command Registry                  │
   │  ┌──────────┐  ┌──────────┐            │
   │  │   led    │  │  button  │            │
   │  └──────────┘  └──────────┘            │
   └─────────────────┬───────────────────────┘
                     │
   ┌─────────────────▼───────────────────────┐
   │       Command Handlers                  │
   │  cmd_led(argc, argv)                    │
   └─────────────────────────────────────────┘

**Key Features**:

* Command registration
* Argument parsing
* Command history
* Tab completion
* Help system

Configuration Framework
~~~~~~~~~~~~~~~~~~~~~~~

**Architecture**:

.. code-block:: text

   ┌─────────────────────────────────────────┐
   │          Application Code               │
   │  config_set_i32("key", value)           │
   └─────────────────┬───────────────────────┘
                     │
   ┌─────────────────▼───────────────────────┐
   │          Config Manager                 │
   │  - Key-value storage                    │
   │  - Type safety                          │
   │  - Namespace support                    │
   └─────────────────┬───────────────────────┘
                     │
   ┌─────────────────▼───────────────────────┐
   │          Storage Backend                │
   │  ┌──────────┐  ┌──────────┐            │
   │  │  Memory  │  │   Flash  │            │
   │  └──────────┘  └──────────┘            │
   └─────────────────────────────────────────┘

**Key Features**:

* Key-value storage
* Multiple data types
* Namespace isolation
* JSON/binary import/export
* Persistence support

Platform Architecture
---------------------

Design Goals
~~~~~~~~~~~~

* Minimal platform-specific code
* Easy to add new platforms
* Consistent initialization
* Resource management

Platform Abstraction
~~~~~~~~~~~~~~~~~~~~

Each platform implements required interfaces:

**Required Implementations**:

* HAL peripheral drivers
* OSAL backend (if using RTOS)
* System initialization
* Clock configuration
* Interrupt handling

**Platform Structure**:

.. code-block:: text

   platforms/stm32/
   ├── src/
   │   ├── stm32f4/
   │   │   ├── gpio_stm32f4.c      # GPIO implementation
   │   │   ├── uart_stm32f4.c      # UART implementation
   │   │   └── system_stm32f4.c    # System init
   │   ├── stm32h7/
   │   │   └── ...
   │   └── common/
   │       └── stm32_common.c      # Common code
   ├── include/
   │   └── stm32_hal.h             # Platform header
   ├── Kconfig                     # Platform config
   └── CMakeLists.txt              # Build config

Initialization Sequence
~~~~~~~~~~~~~~~~~~~~~~~

Platform initialization follows a defined sequence:

.. code-block:: c

   int main(void) {
       /* 1. Platform initialization */
       platform_early_init();      /* Clock, power */

       /* 2. HAL initialization */
       nx_hal_init();              /* HAL subsystem */

       /* 3. OSAL initialization */
       osal_init();                /* OS/scheduler */

       /* 4. Framework initialization */
       log_init(NULL);             /* Logging */
       shell_init(&shell_config);  /* Shell */
       config_init(NULL);          /* Configuration */

       /* 5. Application initialization */
       app_init();

       /* 6. Start scheduler (if using RTOS) */
       osal_start_scheduler();

       /* 7. Main loop (bare-metal) */
       while (1) {
           app_main_loop();
       }

       return 0;
   }

Design Patterns
---------------

Factory Pattern
~~~~~~~~~~~~~~~

**Usage**: Device creation in HAL

**Benefits**:

* Centralized resource management
* Platform-specific instantiation
* Lifecycle management

**Example**: ``nx_factory_gpio_write()``

Adapter Pattern
~~~~~~~~~~~~~~~

**Usage**: OSAL backend adaptation

**Benefits**:

* Support multiple RTOS
* Consistent API
* Easy to add backends

**Example**: FreeRTOS adapter, bare-metal adapter

Strategy Pattern
~~~~~~~~~~~~~~~~

**Usage**: Configurable behavior

**Benefits**:

* Runtime behavior selection
* Easy to add strategies
* Testability

**Example**: Log backends, config storage backends

Observer Pattern
~~~~~~~~~~~~~~~~

**Usage**: Event notification

**Benefits**:

* Loose coupling
* Multiple observers
* Event-driven architecture

**Example**: Interrupt callbacks, event handlers

Singleton Pattern
~~~~~~~~~~~~~~~~~

**Usage**: Global resources

**Benefits**:

* Single instance
* Global access point
* Lazy initialization

**Example**: HAL manager, OSAL scheduler

Design Decisions
----------------

Decision Records
~~~~~~~~~~~~~~~~

Major design decisions are documented as Architecture Decision Records (ADRs).

**ADR Template**:

.. code-block:: text

   # ADR-001: Use Factory Pattern for HAL Devices

   ## Status
   Accepted

   ## Context
   Need a way to create and manage HAL device instances across
   different platforms with resource tracking.

   ## Decision
   Use factory pattern with centralized resource management.

   ## Consequences
   **Positive:**
   - Centralized resource management
   - Platform-specific instantiation
   - Easy to track resources

   **Negative:**
   - Additional indirection
   - Slightly more complex API

   ## Alternatives Considered
   1. Direct instantiation - rejected due to resource management issues
   2. Singleton pattern - rejected due to inflexibility

Key Decisions
~~~~~~~~~~~~~

**1. Layered Architecture**

* **Decision**: Use strict layered architecture
* **Rationale**: Clear separation of concerns, testability
* **Trade-offs**: Some indirection, but worth it for maintainability

**2. Factory Pattern for HAL**

* **Decision**: Use factory pattern for device creation
* **Rationale**: Resource management, platform abstraction
* **Trade-offs**: Additional API layer, but provides flexibility

**3. Adapter Pattern for OSAL**

* **Decision**: Use adapter pattern for OS abstraction
* **Rationale**: Support multiple RTOS, consistent API
* **Trade-offs**: Some overhead, but enables portability

**4. Kconfig for Configuration**

* **Decision**: Use Kconfig for compile-time configuration
* **Rationale**: Proven system, powerful, well-documented
* **Trade-offs**: Learning curve, but very flexible

**5. Property-Based Testing**

* **Decision**: Use property-based testing for HAL
* **Rationale**: Better coverage, finds edge cases
* **Trade-offs**: More complex tests, but higher quality

Performance Considerations
--------------------------

Memory Usage
~~~~~~~~~~~~

**Static Allocation**:

* Predictable memory usage
* No fragmentation
* Suitable for safety-critical systems

**Dynamic Allocation**:

* Flexible memory usage
* Potential fragmentation
* Requires careful management

**Optimization Strategies**:

* Use Kconfig to disable unused features
* Static allocation for resource-constrained systems
* Pool allocation for predictable patterns

CPU Usage
~~~~~~~~~

**Optimization Strategies**:

* Minimize function call overhead
* Use inline functions for hot paths
* Avoid unnecessary copies
* Efficient algorithms

**Profiling**:

* Use profiling tools to identify bottlenecks
* Optimize hot paths
* Balance readability and performance

Code Size
~~~~~~~~~

**Optimization Strategies**:

* Use ``MinSizeRel`` build type
* Disable unused features via Kconfig
* Link-time optimization (LTO)
* Remove debug code in release builds

Future Enhancements
-------------------

Planned Features
~~~~~~~~~~~~~~~~

* **Security**: Secure boot, TLS 1.3, crypto engine
* **Cloud Integration**: AWS IoT, Azure IoT
* **TinyML**: TensorFlow Lite Micro support
* **More Platforms**: ESP32, nRF52
* **More RTOS**: Zephyr, RT-Thread

Architecture Evolution
~~~~~~~~~~~~~~~~~~~~~~

* **Middleware Layer**: Add more middleware components
* **Component System**: Plugin architecture for extensions
* **Service Layer**: High-level services (networking, file system)

See Also
--------

* :doc:`api_design_guidelines` - API design principles
* :doc:`porting_guide` - Platform porting guide
* :doc:`coding_standards` - Coding standards
* :doc:`../user_guide/architecture` - User-facing architecture guide
