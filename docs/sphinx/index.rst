Nexus Embedded Platform
=======================

.. note::

   **Language / 语言**: Use the language switcher in the sidebar to change language.

   **New to Nexus?** Start with :doc:`getting_started/quick_start` or see :doc:`DOCUMENTATION_GUIDE` for navigation help.

Welcome to the Nexus Embedded Platform documentation. Nexus is a world-class
embedded software development platform designed for building reliable,
secure, and portable embedded applications.

.. toctree::
   :maxdepth: 1
   :caption: Documentation

   DOCUMENTATION_GUIDE
   QUICK_REFERENCE

.. toctree::
   :maxdepth: 2
   :caption: Getting Started

   getting_started/index
   getting_started/environment_setup
   getting_started/quick_start
   getting_started/project_structure
   getting_started/build_and_flash
   getting_started/first_application
   getting_started/core_concepts
   getting_started/configuration
   getting_started/examples_tour
   getting_started/faq

.. toctree::
   :maxdepth: 2
   :caption: User Guide

   user_guide/index
   user_guide/architecture
   user_guide/hal
   user_guide/osal
   user_guide/log
   user_guide/shell
   user_guide/config
   user_guide/kconfig
   user_guide/kconfig_tutorial
   user_guide/kconfig_peripherals
   user_guide/kconfig_platforms
   user_guide/kconfig_osal
   user_guide/kconfig_tools
   user_guide/build_system
   user_guide/ide_integration
   user_guide/porting

.. toctree::
   :maxdepth: 2
   :caption: Tutorials

   tutorials/index
   tutorials/first_application
   tutorials/gpio_control
   tutorials/uart_communication
   tutorials/task_creation
   tutorials/interrupt_handling
   tutorials/timer_pwm
   tutorials/spi_communication
   tutorials/examples

.. toctree::
   :maxdepth: 2
   :caption: Platform Guides

   platform_guides/index
   platform_guides/native
   platform_guides/stm32f4
   platform_guides/stm32h7
   platform_guides/gd32

.. toctree::
   :maxdepth: 2
   :caption: API Reference

   api/index
   api/hal
   api/osal
   api/log
   api/shell
   api/config
   api/init
   api/kconfig_tools

.. toctree::
   :maxdepth: 2
   :caption: Reference

   reference/api_index
   reference/kconfig_index
   reference/error_codes

.. toctree::
   :maxdepth: 2
   :caption: Development

   development/index
   development/contributing
   development/coding_standards
   development/testing
   development/kconfig_guide
   development/scripts
   development/validation_framework
   development/script_validation
   development/ci_cd_integration
   development/coverage_analysis
   development/documentation_contributing

Key Features
------------

🔧 Hardware Abstraction Layer (HAL)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Unified API for GPIO, UART, SPI, I2C, Timer, ADC with factory pattern and lifecycle management.

:doc:`Learn more → <user_guide/hal>`

⚙️ OS Abstraction Layer (OSAL)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Portable RTOS interface supporting FreeRTOS, RT-Thread, Zephyr, and bare-metal.

:doc:`Learn more → <user_guide/osal>`

📝 Log Framework
~~~~~~~~~~~~~~~~

Flexible logging with multiple backends, async mode, module filtering, and color output.

:doc:`Learn more → <user_guide/log>`

🖥️ Shell Framework
~~~~~~~~~~~~~~~~~~~

Interactive command-line interface with command registration, history, and auto-completion.

:doc:`Learn more → <user_guide/shell>`

⚙️ Configuration System
~~~~~~~~~~~~~~~~~~~~~~~~

Powerful Kconfig-based configuration with validation, migration, and runtime access.

:doc:`Learn more → <user_guide/kconfig>`

🏗️ Build System
~~~~~~~~~~~~~~~~

CMake-based build system with cross-platform support, toolchain management, and testing.

:doc:`Learn more → <user_guide/build_system>`

🔒 Security & Safety
~~~~~~~~~~~~~~~~~~~~

* Secure boot and firmware updates
* TLS 1.3 and crypto engine
* MISRA C compliance
* MPU protection and runtime checks

☁️ Cloud Integration
~~~~~~~~~~~~~~~~~~~~~

Ready-to-use integrations for AWS IoT, Azure IoT, and Alibaba Cloud IoT.

🤖 AI/ML Support
~~~~~~~~~~~~~~~~

TensorFlow Lite Micro and CMSIS-NN for edge AI applications.

Quick Start
-----------

Get started in 5 minutes:

.. code-block:: bash

   # Clone repository
   git clone https://github.com/X-Gen-Lab/nexus.git
   cd nexus

   # Build for native platform
   python scripts/building/build.py

   # Run example
   ./build/applications/blinky/blinky

:doc:`Full quick start guide → <getting_started/quick_start>`

Supported Platforms
-------------------

* **Native** - Windows, Linux, macOS (simulation)
* **STM32** - F4, H7 series
* **GD32** - GD32F4, GD32F3 series
* **ESP32** - ESP32, ESP32-S3 (coming soon)
* **nRF52** - nRF52832, nRF52840 (coming soon)

:doc:`Platform guides → <platform_guides/index>`

Learning Path
-------------

**New Users** (Week 1)
   1. :doc:`getting_started/quick_start` - Build first example
   2. :doc:`getting_started/first_application` - Create your app
   3. :doc:`tutorials/gpio_control` - First tutorial

**Application Developers** (Week 2-4)
   1. :doc:`user_guide/hal` - Hardware peripherals
   2. :doc:`user_guide/osal` - Task management
   3. :doc:`tutorials/index` - Complete tutorials

**Advanced Users** (Week 5+)
   1. :doc:`user_guide/kconfig` - Advanced configuration
   2. :doc:`development/porting_guide` - Port to new hardware
   3. :doc:`development/contributing` - Contribute code

Community & Support
-------------------

* 📖 **Documentation**: You're reading it!
* 💬 **Discussions**: `GitHub Discussions <https://github.com/X-Gen-Lab/nexus/discussions>`_
* 🐛 **Issues**: `GitHub Issues <https://github.com/X-Gen-Lab/nexus/issues>`_
* 📝 **Changelog**: `CHANGELOG.md <https://github.com/X-Gen-Lab/nexus/blob/main/CHANGELOG.md>`_
* 🤝 **Contributing**: :doc:`development/contributing`

Indices and Tables
==================

* :ref:`genindex` - General index
* :ref:`modindex` - Module index
* :ref:`search` - Search documentation

