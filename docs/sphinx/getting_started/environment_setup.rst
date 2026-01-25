Environment Setup
=================

This guide will help you set up your development environment for Nexus on Windows, Linux, or macOS.

.. contents:: Table of Contents
   :local:
   :depth: 2

System Requirements
-------------------

Minimum Requirements
~~~~~~~~~~~~~~~~~~~~

* **Operating System**: Windows 10+, Ubuntu 20.04+, macOS 11+
* **RAM**: 4 GB minimum, 8 GB recommended
* **Disk Space**: 5 GB for tools and source code
* **Internet**: Required for downloading tools and dependencies

Supported Platforms
~~~~~~~~~~~~~~~~~~~

**Host Development (Native)**

* Windows 10/11 (x64)
* Linux (Ubuntu 20.04+, Debian 11+, Fedora 35+)
* macOS 11+ (Intel and Apple Silicon)

**Target Platforms**

* STM32F4 series (Cortex-M4)
* STM32H7 series (Cortex-M7)
* GD32 series
* Native simulation (for testing)

Required Tools
--------------

Core Tools
~~~~~~~~~~

These tools are required for all platforms:

.. list-table::
   :header-rows: 1
   :widths: 20 20 60

   * - Tool
     - Version
     - Purpose
   * - CMake
     - 3.16+
     - Build system
   * - Git
     - 2.20+
     - Version control
   * - Python
     - 3.8+
     - Build scripts and Kconfig

ARM Cross-Compilation Tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Required for building ARM targets (STM32, GD32):

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Tool
     - Version
     - Purpose
   * - ARM GCC Toolchain
     - 10.3+
     - Cross-compiler for ARM Cortex-M
   * - OpenOCD
     - 0.11+
     - Debugging and flashing (optional)
   * - J-Link
     - Latest
     - Alternative debugger (optional)

Native Build Tools
~~~~~~~~~~~~~~~~~~

For native platform builds:

**Windows**

* Visual Studio 2019+ or MSVC Build Tools
* Or MinGW-w64 (GCC 9+)

**Linux**

* GCC 9+ or Clang 10+
* Build essentials (make, etc.)

**macOS**

* Xcode Command Line Tools (Clang 12+)

Documentation Tools (Optional)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For building documentation:

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - Tool
     - Version
     - Purpose
   * - Doxygen
     - 1.9+
     - API documentation generation
   * - Sphinx
     - 4.0+
     - User guide documentation
   * - Breathe
     - 4.30+
     - Doxygen-Sphinx bridge
   * - sphinx_rtd_theme
     - Latest
     - Read the Docs theme

Installation Instructions
-------------------------

Windows Installation
~~~~~~~~~~~~~~~~~~~~

Using Chocolatey (Recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Install Chocolatey from https://chocolatey.org/install, then:

.. code-block:: powershell

   # Core tools
   choco install cmake git python

   # ARM toolchain
   choco install gcc-arm-embedded

   # Optional: OpenOCD
   choco install openocd

   # Optional: Documentation tools
   choco install doxygen.install

Manual Installation
^^^^^^^^^^^^^^^^^^^

**CMake**

1. Download from https://cmake.org/download/
2. Run installer and add to PATH
3. Verify: ``cmake --version``

**Git**

1. Download from https://git-scm.com/download/win
2. Run installer with default options
3. Verify: ``git --version``

**Python**

1. Download from https://www.python.org/downloads/
2. Run installer and check "Add Python to PATH"
3. Verify: ``python --version``

**ARM GCC Toolchain**

1. Download from https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm
2. Extract to ``C:\Program Files (x86)\GNU Arm Embedded Toolchain``
3. Add ``bin`` directory to PATH
4. Verify: ``arm-none-eabi-gcc --version``

**Visual Studio Build Tools**

1. Download from https://visualstudio.microsoft.com/downloads/
2. Install "Desktop development with C++" workload
3. Or install full Visual Studio 2019/2022

Linux Installation
~~~~~~~~~~~~~~~~~~

Ubuntu/Debian
^^^^^^^^^^^^^

.. code-block:: bash

   # Update package list
   sudo apt update

   # Core tools
   sudo apt install cmake git python3 python3-pip build-essential

   # ARM toolchain
   sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi \
                    libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib

   # Optional: OpenOCD
   sudo apt install openocd

   # Optional: Documentation tools
   sudo apt install doxygen graphviz
   pip3 install sphinx breathe sphinx_rtd_theme

Fedora/RHEL
^^^^^^^^^^^

.. code-block:: bash

   # Core tools
   sudo dnf install cmake git python3 python3-pip gcc gcc-c++ make

   # ARM toolchain
   sudo dnf install arm-none-eabi-gcc-cs arm-none-eabi-binutils \
                    arm-none-eabi-newlib

   # Optional: OpenOCD
   sudo dnf install openocd

   # Optional: Documentation tools
   sudo dnf install doxygen graphviz
   pip3 install sphinx breathe sphinx_rtd_theme

Arch Linux
^^^^^^^^^^

.. code-block:: bash

   # Core tools
   sudo pacman -S cmake git python python-pip base-devel

   # ARM toolchain
   sudo pacman -S arm-none-eabi-gcc arm-none-eabi-binutils \
                  arm-none-eabi-newlib

   # Optional: OpenOCD
   sudo pacman -S openocd

   # Optional: Documentation tools
   sudo pacman -S doxygen graphviz
   pip install sphinx breathe sphinx_rtd_theme

macOS Installation
~~~~~~~~~~~~~~~~~~

Using Homebrew (Recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Install Homebrew from https://brew.sh/, then:

.. code-block:: bash

   # Core tools
   brew install cmake git python

   # ARM toolchain
   brew install --cask gcc-arm-embedded

   # Or using tap
   brew tap osx-cross/arm
   brew install arm-gcc-bin

   # Optional: OpenOCD
   brew install openocd

   # Optional: Documentation tools
   brew install doxygen graphviz
   pip3 install sphinx breathe sphinx_rtd_theme

Xcode Command Line Tools
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: bash

   # Install Xcode Command Line Tools
   xcode-select --install

   # Verify
   clang --version

Python Dependencies
-------------------

Install Python packages required for build scripts:

.. code-block:: bash

   # Navigate to Nexus directory
   cd nexus

   # Install requirements
   pip install -r requirements.txt

   # Or install manually
   pip install kconfiglib colorama

Verify Installation
-------------------

Check All Tools
~~~~~~~~~~~~~~~

Run this script to verify all tools are installed:

.. code-block:: bash

   # Check CMake
   cmake --version

   # Check Git
   git --version

   # Check Python
   python --version
   python -c "import kconfiglib; print('kconfiglib OK')"

   # Check ARM toolchain
   arm-none-eabi-gcc --version
   arm-none-eabi-objcopy --version
   arm-none-eabi-size --version

   # Check native compiler
   # Windows (MSVC)
   cl.exe

   # Linux/macOS (GCC)
   gcc --version

   # macOS (Clang)
   clang --version

Expected Output
~~~~~~~~~~~~~~~

You should see version information for each tool:

.. code-block:: text

   cmake version 3.22.1
   git version 2.34.1
   Python 3.10.4
   kconfiglib OK
   arm-none-eabi-gcc (GNU Arm Embedded Toolchain 10.3-2021.10) 10.3.1
   gcc (Ubuntu 11.2.0-19ubuntu1) 11.2.0

Clone Nexus Repository
-----------------------

Get the Source Code
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Clone repository
   git clone https://github.com/nexus-platform/nexus.git
   cd nexus

   # Initialize submodules
   git submodule update --init --recursive

Verify Repository
~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Check directory structure
   ls -la

   # You should see:
   # - hal/
   # - osal/
   # - framework/
   # - platforms/
   # - applications/
   # - tests/
   # - CMakeLists.txt
   # - Kconfig
   # - README.md

IDE Setup (Optional)
--------------------

Visual Studio Code
~~~~~~~~~~~~~~~~~~

Recommended extensions:

* C/C++ (Microsoft)
* CMake Tools (Microsoft)
* CMake (twxs)
* Cortex-Debug (for ARM debugging)
* Python (Microsoft)

Install extensions:

.. code-block:: bash

   code --install-extension ms-vscode.cpptools
   code --install-extension ms-vscode.cmake-tools
   code --install-extension twxs.cmake
   code --install-extension marus25.cortex-debug
   code --install-extension ms-python.python

See :doc:`../user_guide/ide_integration` for detailed VS Code setup.

CLion
~~~~~

1. Open Nexus directory as CMake project
2. Configure CMake profiles for different platforms
3. Set up toolchains in Settings → Build, Execution, Deployment → Toolchains

Eclipse
~~~~~~~

1. Install Eclipse IDE for C/C++ Developers
2. Import as CMake project
3. Configure ARM toolchain in Project Properties

Hardware Setup
--------------

STM32F4 Discovery Board
~~~~~~~~~~~~~~~~~~~~~~~

**What You Need**

* STM32F4DISCOVERY board (STM32F407VGT6)
* Mini-USB cable
* Windows: ST-Link driver from https://www.st.com/en/development-tools/stsw-link009.html

**Board Features**

* 4 user LEDs (PD12-PD15)
* 1 user button (PA0)
* ST-LINK/V2 debugger (built-in)
* Arduino-compatible headers

**Driver Installation**

Windows:

1. Download ST-Link driver
2. Install driver
3. Connect board via USB
4. Verify in Device Manager

Linux:

.. code-block:: bash

   # Install udev rules
   sudo cp 49-stlinkv2.rules /etc/udev/rules.d/
   sudo udevadm control --reload-rules

macOS:

No driver needed, works out of the box.

**Test Connection**

.. code-block:: bash

   # Using OpenOCD
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

   # Using ST-Link utilities
   st-info --probe

Other Boards
~~~~~~~~~~~~

See platform-specific guides:

* :doc:`../platform_guides/stm32f4` - STM32F4 boards
* :doc:`../platform_guides/stm32h7` - STM32H7 boards
* :doc:`../platform_guides/gd32` - GD32 boards

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**CMake not found**

.. code-block:: text

   'cmake' is not recognized as an internal or external command

Solution: Add CMake to PATH or reinstall with "Add to PATH" option.

**ARM toolchain not found**

.. code-block:: text

   Could not find arm-none-eabi-gcc

Solution: Install ARM toolchain and add to PATH.

**Python module not found**

.. code-block:: text

   ModuleNotFoundError: No module named 'kconfiglib'

Solution: Install Python dependencies:

.. code-block:: bash

   pip install -r requirements.txt

**ST-Link not detected (Windows)**

Solution: Install ST-Link driver from ST website.

**Permission denied (Linux)**

.. code-block:: text

   Error: libusb_open() failed with LIBUSB_ERROR_ACCESS

Solution: Add udev rules and add user to dialout group:

.. code-block:: bash

   sudo usermod -a -G dialout $USER
   # Log out and log back in

Getting Help
~~~~~~~~~~~~

If you encounter issues:

1. Check :doc:`faq` for common problems
2. Search `GitHub Issues <https://github.com/nexus-platform/nexus/issues>`_
3. Ask in `GitHub Discussions <https://github.com/nexus-platform/nexus/discussions>`_
4. Review :doc:`../development/contributing` for contribution guidelines

Next Steps
----------

Now that your environment is set up:

1. :doc:`quick_start` - Build your first example
2. :doc:`project_structure` - Understand the codebase
3. :doc:`build_and_flash` - Learn build and deployment workflows

See Also
--------

* :doc:`../user_guide/ide_integration` - Detailed IDE setup
* :doc:`../user_guide/build_system` - Build system documentation
* :doc:`../platform_guides/index` - Platform-specific guides
