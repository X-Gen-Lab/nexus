Nexus Embedded Platform
=======================

.. note::

   **Language / 语言**: Use the language switcher in the sidebar.

Welcome to the Nexus Embedded Platform documentation. Nexus is a world-class
embedded software development platform designed for building reliable,
secure, and portable embedded applications.

.. toctree::
   :maxdepth: 2
   :caption: Getting Started

   getting_started/introduction
   getting_started/installation
   getting_started/quickstart

.. toctree::
   :maxdepth: 2
   :caption: User Guide

   user_guide/architecture
   user_guide/hal
   user_guide/osal
   user_guide/log
   user_guide/shell
   user_guide/config
   user_guide/porting

.. toctree::
   :maxdepth: 2
   :caption: API Reference

   api/hal
   api/osal
   api/log
   api/shell
   api/config

.. toctree::
   :maxdepth: 2
   :caption: Development

   development/contributing
   development/coding_standards
   development/testing

Features
--------

* **Hardware Abstraction Layer (HAL)**: Unified API for GPIO, UART, SPI, I2C, Timer, ADC
* **OS Abstraction Layer (OSAL)**: Portable RTOS interface supporting FreeRTOS and bare-metal
* **Log Framework**: Flexible logging with multiple backends, async mode, and module filtering
* **Multi-Platform Support**: STM32F4, STM32H7, ESP32, nRF52, and more
* **Security**: Secure boot, TLS 1.3, crypto engine, key management
* **Functional Safety**: MISRA C compliance, MPU protection, runtime checks
* **Cloud Integration**: AWS IoT, Azure IoT, Alibaba Cloud IoT
* **AI/ML Support**: TensorFlow Lite Micro, CMSIS-NN

Quick Links
-----------

* :doc:`getting_started/introduction` - Learn about Nexus
* :doc:`getting_started/installation` - Set up your environment
* :doc:`getting_started/quickstart` - Build your first app

Community
---------

* `GitHub <https://github.com/X-Gen-Lab/nexus>`_
* `Issues <https://github.com/X-Gen-Lab/nexus/issues>`_
* `Discussions <https://github.com/X-Gen-Lab/nexus/discussions>`_
* `Changelog <https://github.com/X-Gen-Lab/nexus/blob/main/CHANGELOG.md>`_

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
