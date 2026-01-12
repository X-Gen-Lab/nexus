Installation
============

Prerequisites
-------------

Before installing Nexus, ensure you have the following tools:

**Required:**

- CMake 3.16 or later
- C compiler (GCC, Clang, or MSVC)
- Git

**For ARM targets:**

- ARM GCC toolchain (arm-none-eabi-gcc)
- OpenOCD or J-Link for debugging

**For documentation:**

- Doxygen 1.9+
- Python 3.8+
- Sphinx and Breathe

Installing Prerequisites
------------------------

**Windows:**

.. code-block:: powershell

    # Install with Chocolatey
    choco install cmake git
    choco install gcc-arm-embedded

**Linux (Ubuntu/Debian):**

.. code-block:: bash

    sudo apt update
    sudo apt install cmake git build-essential
    sudo apt install gcc-arm-none-eabi

**macOS:**

.. code-block:: bash

    brew install cmake git
    brew install --cask gcc-arm-embedded

Getting Nexus
-------------

Clone the repository:

.. code-block:: bash

    git clone https://github.com/nexus-team/nexus.git
    cd nexus

Building
--------

**Native build (for testing):**

.. code-block:: bash

    cmake -B build -DNEXUS_PLATFORM=native
    cmake --build build

**STM32F4 cross-compile:**

.. code-block:: bash

    cmake -B build-stm32f4 \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
        -DNEXUS_PLATFORM=stm32f4
    cmake --build build-stm32f4

Running Tests
-------------

.. code-block:: bash

    cd build
    ctest --output-on-failure

Building Documentation
----------------------

.. code-block:: bash

    # Generate API docs
    doxygen Doxyfile

    # Build Sphinx docs
    cd docs/sphinx
    sphinx-build -b html . _build/html

Next Steps
----------

- :doc:`quickstart` - Build your first Nexus application
- :doc:`../user_guide/architecture` - Understand the platform architecture
