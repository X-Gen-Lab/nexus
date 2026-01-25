Documentation Guide
===================

Welcome to the Nexus documentation! This guide helps you navigate and find the information you need.

.. contents:: Quick Navigation
   :local:
   :depth: 2

Documentation Structure
-----------------------

The Nexus documentation is organized into five main sections:

📚 Getting Started
~~~~~~~~~~~~~~~~~~

**For**: New users, first-time setup

**Contains**:

* Installation and environment setup
* Quick start guide (5 minutes)
* Project structure overview
* Building and flashing
* Core concepts introduction
* Configuration basics
* Example applications tour
* FAQ and troubleshooting

**Start here**: :doc:`getting_started/index`

📖 User Guide
~~~~~~~~~~~~~

**For**: Application developers, daily usage

**Contains**:

* Architecture overview
* HAL (Hardware Abstraction Layer)
* OSAL (OS Abstraction Layer)
* Framework components (Log, Shell, Config)
* Kconfig configuration system
* Build system documentation
* IDE integration
* Platform-specific guides

**Start here**: :doc:`user_guide/index`

🎓 Tutorials
~~~~~~~~~~~~

**For**: Hands-on learning, practical examples

**Contains**:

* Step-by-step tutorials
* GPIO control
* UART communication
* Task creation
* Interrupt handling
* Timer and PWM
* SPI communication
* Complete example projects

**Start here**: :doc:`tutorials/index`

🔧 Platform Guides
~~~~~~~~~~~~~~~~~~

**For**: Platform-specific information

**Contains**:

* Native platform (simulation)
* STM32F4 series
* STM32H7 series
* GD32 series
* Platform-specific features
* Hardware setup guides

**Start here**: :doc:`platform_guides/index`

📋 API Reference
~~~~~~~~~~~~~~~~

**For**: Detailed API documentation

**Contains**:

* HAL API reference
* OSAL API reference
* Framework API reference
* Configuration options
* Error codes
* Kconfig reference

**Start here**: :doc:`api/index`

🛠️ Development
~~~~~~~~~~~~~~~

**For**: Contributors, maintainers, porters

**Contains**:

* Contributing guidelines
* Coding standards
* Testing framework
* Build system internals
* Porting guide
* CI/CD integration
* Release process

**Start here**: :doc:`development/index`

Finding What You Need
---------------------

By Role
~~~~~~~

**I'm a beginner**
   1. :doc:`getting_started/quick_start` - Get running in 5 minutes
   2. :doc:`getting_started/first_application` - Create your first app
   3. :doc:`tutorials/gpio_control` - First tutorial

**I'm building an application**
   1. :doc:`user_guide/hal` - Hardware peripherals
   2. :doc:`user_guide/osal` - Task management
   3. :doc:`user_guide/config` - Configuration system

**I'm porting to new hardware**
   1. :doc:`development/porting_guide` - Porting guide
   2. :doc:`user_guide/architecture` - Architecture overview
   3. :doc:`platform_guides/index` - Platform examples

**I'm contributing code**
   1. :doc:`development/contributing` - Contribution guide
   2. :doc:`development/coding_standards` - Code style
   3. :doc:`development/testing` - Testing requirements

By Task
~~~~~~~

**Setting up environment**
   → :doc:`getting_started/environment_setup`

**Building project**
   → :doc:`getting_started/build_and_flash`

**Configuring features**
   → :doc:`user_guide/kconfig`

**Using GPIO**
   → :doc:`tutorials/gpio_control` or :doc:`user_guide/hal`

**Using UART**
   → :doc:`tutorials/uart_communication` or :doc:`user_guide/hal`

**Creating tasks**
   → :doc:`tutorials/task_creation` or :doc:`user_guide/osal`

**Debugging issues**
   → :doc:`user_guide/debugging` or :doc:`getting_started/faq`

**Writing tests**
   → :doc:`development/testing`

**Adding new platform**
   → :doc:`development/porting_guide`

By Topic
~~~~~~~~

**Architecture & Design**
   * :doc:`user_guide/architecture`
   * :doc:`development/architecture_design`
   * :doc:`development/api_design_guidelines`

**Hardware Abstraction**
   * :doc:`user_guide/hal`
   * :doc:`api/hal`
   * :doc:`user_guide/kconfig_peripherals`

**Operating System**
   * :doc:`user_guide/osal`
   * :doc:`api/osal`
   * :doc:`user_guide/kconfig_osal`

**Configuration**
   * :doc:`user_guide/kconfig`
   * :doc:`user_guide/kconfig_tutorial`
   * :doc:`development/kconfig_guide`

**Build System**
   * :doc:`user_guide/build_system`
   * :doc:`development/build_system`
   * :doc:`getting_started/build_and_flash`

**Testing & Quality**
   * :doc:`development/testing`
   * :doc:`development/coverage_analysis`
   * :doc:`development/validation_framework`

**Platform Support**
   * :doc:`platform_guides/native`
   * :doc:`platform_guides/stm32f4`
   * :doc:`platform_guides/stm32h7`
   * :doc:`platform_guides/gd32`

Search Tips
-----------

Use the search box in the sidebar to find specific topics:

**Search by keyword**:
   * "GPIO" - Find all GPIO-related documentation
   * "UART" - Find UART documentation
   * "Kconfig" - Find configuration documentation

**Search by function name**:
   * "nx_factory_gpio" - Find GPIO factory function
   * "nx_hal_init" - Find HAL initialization

**Search by error code**:
   * "NX_ERR_PARAM" - Find error code documentation

**Search by platform**:
   * "STM32F4" - Find STM32F4-specific documentation

Language Selection
------------------

Nexus documentation is available in multiple languages:

* **English** (en) - Default language
* **中文** (zh_CN) - Chinese (Simplified)

Use the language switcher in the sidebar to change languages.

Documentation Conventions
-------------------------

Throughout the documentation, we use these conventions:

**Code Examples**

.. code-block:: c

   /* C code examples */
   int main(void) {
       return 0;
   }

**Shell Commands**

.. code-block:: bash

   # Shell commands
   cmake -B build

**Configuration Options**

.. code-block:: kconfig

   # Kconfig options
   CONFIG_HAL_GPIO=y

**Notes**

.. note::

   Important information or tips

**Warnings**

.. warning::

   Critical information or potential issues

**Tips**

.. tip::

   Helpful suggestions or best practices

**Cross-references**

Links to other documentation: :doc:`getting_started/index`

**External links**

Links to external resources: `GitHub <https://github.com/X-Gen-Lab/nexus>`_

Getting Help
------------

If you can't find what you need:

1. **Search the documentation** - Use the search box
2. **Check the FAQ** - :doc:`getting_started/faq`
3. **Browse examples** - :doc:`getting_started/examples_tour`
4. **Ask the community** - `GitHub Discussions <https://github.com/X-Gen-Lab/nexus/discussions>`_
5. **Report issues** - `GitHub Issues <https://github.com/X-Gen-Lab/nexus/issues>`_

Contributing to Documentation
------------------------------

Found an error or want to improve the documentation?

See :doc:`development/documentation_contributing` for guidelines on:

* Reporting documentation issues
* Suggesting improvements
* Writing new documentation
* Translating documentation

Quick Reference
---------------

**Most Common Pages**

* :doc:`getting_started/quick_start` - Quick start guide
* :doc:`user_guide/hal` - HAL documentation
* :doc:`user_guide/osal` - OSAL documentation
* :doc:`user_guide/kconfig` - Configuration guide
* :doc:`api/index` - API reference
* :doc:`getting_started/faq` - FAQ

**External Resources**

* `GitHub Repository <https://github.com/X-Gen-Lab/nexus>`_
* `Issue Tracker <https://github.com/X-Gen-Lab/nexus/issues>`_
* `Discussions <https://github.com/X-Gen-Lab/nexus/discussions>`_
* `Changelog <https://github.com/X-Gen-Lab/nexus/blob/main/CHANGELOG.md>`_

Offline Documentation
---------------------

To build and use documentation offline:

.. code-block:: bash

   # Build HTML documentation
   cd docs/sphinx
   python build_docs.py

   # Build PDF documentation (requires LaTeX)
   python build_docs.py --format pdf

   # Serve locally
   python build_docs.py --serve

Output will be in ``docs/sphinx/_build/html/``.

Documentation Versions
----------------------

* **Latest** - Development version (main branch)
* **Stable** - Latest release version
* **Archive** - Previous versions

See the version selector (if available) to switch between versions.

Feedback
--------

Your feedback helps improve the documentation!

* Found an error? `Report it <https://github.com/X-Gen-Lab/nexus/issues>`_
* Have a suggestion? `Discuss it <https://github.com/X-Gen-Lab/nexus/discussions>`_
* Want to contribute? See :doc:`development/documentation_contributing`

Thank you for using Nexus! 🚀
