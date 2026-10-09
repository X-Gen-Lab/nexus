Nexus Embedded Platform
=======================

.. note::

   **Language / 语言**: Use the language switcher in the sidebar to change language.

   **New to Nexus?** Start with :doc:`getting_started/quick_start` or see :doc:`DOCUMENTATION_GUIDE` for navigation help.

Nexus provides reusable embedded platform contracts, SoC/Board integration,
explicit HAL/OSAL lifecycle, common components and traceable builds. Applications
and product policy live in external repositories. Source-bound software checks
and physical hardware qualification are separate gates.

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

Opaque device ownership and independent typed GPIO/UART/SPI/I2C/Flash facades.
Native typed I2C is implemented; STM32F407/GD32F470 typed I2C and UART DMA are
unsupported. Legacy virtual peripherals do not establish MCU support.

:doc:`Learn more → <user_guide/hal>`

⚙️ OS Abstraction Layer (OSAL)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Explicit Native, baremetal and pinned FreeRTOS backends with capability/resource
queries. RT-Thread and Zephyr are not maintained backends.

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

One per-build Kconfig effective configuration with strict validation. Runtime
Config storage is a separate explicitly selected component.

:doc:`Learn more → <user_guide/kconfig>`

🏗️ Build System
~~~~~~~~~~~~~~~~

CMake-based build system with cross-platform support, toolchain management, and testing.

:doc:`Learn more → <user_guide/build_system>`

🔒 Security & Safety
~~~~~~~~~~~~~~~~~~~~

Security core has no automatic provider; OpenSSL is an explicit Native choice.
MCU bootloader/trust chain, crypto/entropy/vault, MPU and industrial safety
qualification are not implemented or qualified by this software delivery.

Quick Start
-----------

Get started in 5 minutes:

.. code-block:: bash

   # Clone repository
   git clone --recurse-submodules https://github.com/X-Gen-Lab/nexus.git
   cd nexus

   # Build for native platform
   python3 -m pip install kconfiglib==14.1.0
   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4

:doc:`Full quick start guide → <getting_started/quick_start>`

Supported Platforms
-------------------

* **Native** - Linux executed host models; Windows/macOS presets retained.
* **STM32F407VE/VG/ZG** - Sky Youth, Discovery and Qiming V3.1.
* **GD32F470ZG** - Liangshan Pi using independent official SDK integration.

MCU baremetal/FreeRTOS software integration is maintained. The user has deferred
boards; no physical probe, flashing, serial, waveform, power-loss or long-load
qualification has been executed. Exact current evidence and known limits are in
``docs/strategy/support-matrix.yaml`` and external examples validation records.

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

