Development Environment
=======================

This guide covers setting up a complete development environment for Nexus, including IDEs, debuggers, and development tools.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

A proper development environment significantly improves productivity. This guide covers:

* IDE configuration (VS Code, CLion, Eclipse)
* Debugger setup (GDB, OpenOCD, J-Link)
* Code analysis tools
* Documentation tools
* Platform-specific configurations

Prerequisites
-------------

Before setting up your development environment, ensure you have completed:

* :doc:`../getting_started/environment_setup` - Basic tool installation
* :doc:`../getting_started/quick_start` - Verified build works

IDE Setup
---------

Visual Studio Code
~~~~~~~~~~~~~~~~~~

VS Code is the recommended IDE for Nexus development.

Installation
^^^^^^^^^^^^

**Windows**:

.. code-block:: powershell

   winget install Microsoft.VisualStudioCode

**Linux**:

.. code-block:: bash

   sudo snap install code --classic

**macOS**:

.. code-block:: bash

   brew install --cask visual-studio-code

Required Extensions
^^^^^^^^^^^^^^^^^^^

Install these extensions for optimal development:

.. code-block:: bash

   # C/C++ development
   code --install-extension ms-vscode.cpptools
   code --install-extension ms-vscode.cpptools-extension-pack
   code --install-extension ms-vscode.cmake-tools
   code --install-extension twxs.cmake

   # Debugging
   code --install-extension marus25.cortex-debug

   # Code quality
   code --install-extension llvm-vs-code-extensions.vscode-clangd
   code --install-extension notskm.clang-tidy

   # Documentation
   code --install-extension cschlosser.doxdocgen
   code --install-extension lextudio.restructuredtext

   # Git
   code --install-extension eamodio.gitlens

   # Python (for scripts)
   code --install-extension ms-python.python

Workspace Configuration
^^^^^^^^^^^^^^^^^^^^^^^

Nexus includes pre-configured VS Code settings in ``.vscode/``:

.. code-block:: text

   .vscode/
   ├── settings.json          # Workspace settings
   ├── tasks.json             # Build tasks
   ├── launch.json            # Debug configurations
   ├── c_cpp_properties.json  # IntelliSense configuration
   └── extensions.json        # Recommended extensions

**settings.json** - Key settings:

.. code-block:: json

   {
       "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
       "cmake.configureOnOpen": true,
       "cmake.buildDirectory": "${workspaceFolder}/build",
       "editor.formatOnSave": true,
       "editor.rulers": [80],
       "files.trimTrailingWhitespace": true,
       "files.insertFinalNewline": true,
       "C_Cpp.clang_format_style": "file",
       "clang-tidy.executable": "clang-tidy",
       "clang-tidy.lintOnSave": true
   }

**tasks.json** - Build tasks:

.. code-block:: json

   {
       "version": "2.0.0",
       "tasks": [
           {
               "label": "Build (Native Debug)",
               "type": "shell",
               "command": "python",
               "args": ["scripts/building/build.py", "-t", "debug"],
               "group": {
                   "kind": "build",
                   "isDefault": true
               }
           },
           {
               "label": "Build (STM32F4 Release)",
               "type": "shell",
               "command": "python",
               "args": [
                   "scripts/building/build.py",
                   "--platform", "stm32f4",
                   "-t", "release"
               ]
           },
           {
               "label": "Run Tests",
               "type": "shell",
               "command": "python",
               "args": ["scripts/test/test.py"]
           },
           {
               "label": "Format Code",
               "type": "shell",
               "command": "python",
               "args": ["scripts/tools/format.py"]
           },
           {
               "label": "Generate Documentation",
               "type": "shell",
               "command": "python",
               "args": ["scripts/tools/docs.py"]
           }
       ]
   }

**launch.json** - Debug configurations:

.. code-block:: json

   {
       "version": "0.2.0",
       "configurations": [
           {
               "name": "Debug Native Application",
               "type": "cppdbg",
               "request": "launch",
               "program": "${workspaceFolder}/build/applications/blinky/blinky",
               "args": [],
               "stopAtEntry": false,
               "cwd": "${workspaceFolder}",
               "environment": [],
               "externalConsole": false,
               "MIMode": "gdb",
               "setupCommands": [
                   {
                       "description": "Enable pretty-printing for gdb",
                       "text": "-enable-pretty-printing",
                       "ignoreFailures": true
                   }
               ]
           },
           {
               "name": "Debug STM32F4 (OpenOCD)",
               "type": "cortex-debug",
               "request": "launch",
               "servertype": "openocd",
               "cwd": "${workspaceFolder}",
               "executable": "${workspaceFolder}/build-stm32f4/applications/blinky/blinky.elf",
               "device": "STM32F407VG",
               "configFiles": [
                   "interface/stlink.cfg",
                   "target/stm32f4x.cfg"
               ],
               "svdFile": "${workspaceFolder}/vendors/st/cmsis/stm32f4xx.svd",
               "runToMain": true,
               "preLaunchTask": "Build (STM32F4 Release)"
           },
           {
               "name": "Debug STM32F4 (J-Link)",
               "type": "cortex-debug",
               "request": "launch",
               "servertype": "jlink",
               "cwd": "${workspaceFolder}",
               "executable": "${workspaceFolder}/build-stm32f4/applications/blinky/blinky.elf",
               "device": "STM32F407VG",
               "interface": "swd",
               "runToMain": true
           }
       ]
   }

**c_cpp_properties.json** - IntelliSense:

.. code-block:: json

   {
       "configurations": [
           {
               "name": "Native",
               "includePath": [
                   "${workspaceFolder}/**",
                   "${workspaceFolder}/hal/include",
                   "${workspaceFolder}/osal/include",
                   "${workspaceFolder}/framework/*/include"
               ],
               "defines": [
                   "NEXUS_PLATFORM_NATIVE",
                   "DEBUG"
               ],
               "compilerPath": "/usr/bin/gcc",
               "cStandard": "c11",
               "cppStandard": "c++17",
               "intelliSenseMode": "gcc-x64",
               "configurationProvider": "ms-vscode.cmake-tools"
           },
           {
               "name": "STM32F4",
               "includePath": [
                   "${workspaceFolder}/**",
                   "${workspaceFolder}/hal/include",
                   "${workspaceFolder}/osal/include",
                   "${workspaceFolder}/vendors/st/cmsis/Include"
               ],
               "defines": [
                   "NEXUS_PLATFORM_STM32F4",
                   "STM32F407xx",
                   "USE_HAL_DRIVER"
               ],
               "compilerPath": "/usr/bin/arm-none-eabi-gcc",
               "cStandard": "c11",
               "cppStandard": "c++17",
               "intelliSenseMode": "gcc-arm"
           }
       ],
       "version": 4
   }

Keyboard Shortcuts
^^^^^^^^^^^^^^^^^^

Useful VS Code shortcuts for Nexus development:

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Shortcut
     - Action
   * - ``Ctrl+Shift+B``
     - Run build task
   * - ``F5``
     - Start debugging
   * - ``Ctrl+Shift+P``
     - Command palette
   * - ``Ctrl+P``
     - Quick file open
   * - ``Ctrl+Shift+F``
     - Search in files
   * - ``F12``
     - Go to definition
   * - ``Shift+F12``
     - Find all references
   * - ``Ctrl+K Ctrl+F``
     - Format selection
   * - ``Ctrl+/``
     - Toggle comment

CLion
~~~~~

CLion is a powerful C/C++ IDE from JetBrains.

Installation
^^^^^^^^^^^^

Download from https://www.jetbrains.com/clion/

**Student License**: Free for students with .edu email

Configuration
^^^^^^^^^^^^^

1. **Open Project**:

   * File → Open → Select ``nexus`` directory
   * CLion will detect CMakeLists.txt automatically

2. **Configure CMake**:

   * File → Settings → Build, Execution, Deployment → CMake
   * Add profiles:

     * Debug (Native): ``-DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON``
     * Release (Native): ``-DNEXUS_PLATFORM=native -DCMAKE_BUILD_TYPE=Release``
     * STM32F4: ``-DNEXUS_PLATFORM=stm32f4 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake``

3. **Configure Toolchains**:

   * File → Settings → Build, Execution, Deployment → Toolchains
   * Add ARM toolchain:

     * Name: ARM GCC
     * CMake: System CMake
     * Make: System Make
     * C Compiler: ``/usr/bin/arm-none-eabi-gcc``
     * C++ Compiler: ``/usr/bin/arm-none-eabi-g++``
     * Debugger: ``/usr/bin/arm-none-eabi-gdb``

4. **Configure Code Style**:

   * File → Settings → Editor → Code Style → C/C++
   * Scheme: Project
   * Import ``.clang-format`` settings

5. **Configure External Tools**:

   * File → Settings → Tools → External Tools
   * Add tools:

     * Format Code: ``python scripts/tools/format.py``
     * Run Tests: ``python scripts/test/test.py``
     * Generate Docs: ``python scripts/tools/docs.py``

Run Configurations
^^^^^^^^^^^^^^^^^^

Create run configurations for common tasks:

**Build Configuration**:

* Name: Build Native
* Target: All targets
* Configuration: Debug

**Test Configuration**:

* Name: Run Tests
* Target: nexus_tests
* Configuration: Debug
* Program arguments: ``--gtest_output=xml:test_results.xml``

**Debug Configuration**:

* Name: Debug Blinky
* Target: blinky
* Configuration: Debug

Eclipse
~~~~~~~

Eclipse CDT is a popular open-source IDE.

Installation
^^^^^^^^^^^^

**Linux**:

.. code-block:: bash

   sudo apt install eclipse-cdt

**Windows/macOS**: Download from https://www.eclipse.org/downloads/

Configuration
^^^^^^^^^^^^^

1. **Import Project**:

   * File → Import → C/C++ → Existing Code as Makefile Project
   * Browse to ``nexus`` directory
   * Toolchain: Linux GCC or Cross GCC

2. **Configure CMake**:

   * Project → Properties → C/C++ Build
   * Builder Settings:

     * Build command: ``cmake --build build``
     * Build directory: ``${workspace_loc:/nexus}/build``

3. **Configure Indexer**:

   * Project → Properties → C/C++ General → Paths and Symbols
   * Add include paths:

     * ``${workspace_loc:/nexus}/hal/include``
     * ``${workspace_loc:/nexus}/osal/include``
     * ``${workspace_loc:/nexus}/framework/*/include``

4. **Configure Code Style**:

   * Window → Preferences → C/C++ → Code Style → Formatter
   * Import ``.clang-format`` settings

Debugger Setup
--------------

GDB (GNU Debugger)
~~~~~~~~~~~~~~~~~~

GDB is the standard debugger for C/C++ on Linux.

Installation
^^^^^^^^^^^^

**Linux**:

.. code-block:: bash

   sudo apt install gdb

**macOS**:

.. code-block:: bash

   brew install gdb

**Windows**: Included with MinGW or use WSL

Configuration
^^^^^^^^^^^^^

Create ``~/.gdbinit`` for custom settings:

.. code-block:: text

   # Enable pretty printing
   set print pretty on
   set print object on
   set print static-members on
   set print vtbl on
   set print demangle on
   set demangle-style gnu-v3

   # History
   set history save on
   set history size 10000
   set history filename ~/.gdb_history

   # Auto-load safe path
   add-auto-load-safe-path /path/to/nexus

Basic Usage
^^^^^^^^^^^

.. code-block:: bash

   # Start GDB with program
   gdb ./build/applications/blinky/blinky

   # GDB commands
   (gdb) break main              # Set breakpoint at main
   (gdb) run                     # Run program
   (gdb) next                    # Step over
   (gdb) step                    # Step into
   (gdb) continue                # Continue execution
   (gdb) print variable          # Print variable value
   (gdb) backtrace               # Show call stack
   (gdb) info locals             # Show local variables
   (gdb) quit                    # Exit GDB

Advanced GDB
^^^^^^^^^^^^

**Conditional Breakpoints**:

.. code-block:: text

   (gdb) break hal_gpio_init if port == 5

**Watchpoints**:

.. code-block:: text

   (gdb) watch variable          # Break when variable changes
   (gdb) rwatch variable         # Break when variable is read
   (gdb) awatch variable         # Break on read or write

**Pretty Printers**:

Create ``.gdbinit`` in project root:

.. code-block:: python

   import sys
   sys.path.insert(0, '/path/to/nexus/scripts/gdb')
   import nexus_printers
   nexus_printers.register_printers()

OpenOCD
~~~~~~~

OpenOCD provides on-chip debugging for ARM targets.

Installation
^^^^^^^^^^^^

**Linux**:

.. code-block:: bash

   sudo apt install openocd

**macOS**:

.. code-block:: bash

   brew install openocd

**Windows**: Download from https://openocd.org/

Configuration
^^^^^^^^^^^^^

Create ``openocd.cfg`` for your board:

.. code-block:: text

   # STM32F4 Discovery
   source [find interface/stlink.cfg]
   source [find target/stm32f4x.cfg]

   # Reset configuration
   reset_config srst_only

   # Flash programming
   $_TARGETNAME configure -event reset-init {
       # Configure clocks
       mww 0x40023C00 0x00000003  # Enable HSI
   }

Usage
^^^^^

**Start OpenOCD**:

.. code-block:: bash

   # Using config file
   openocd -f openocd.cfg

   # Or specify interface and target
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

**Connect GDB**:

Terminal 1 (OpenOCD):

.. code-block:: bash

   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

Terminal 2 (GDB):

.. code-block:: bash

   arm-none-eabi-gdb build-stm32f4/applications/blinky/blinky.elf
   (gdb) target remote localhost:3333
   (gdb) monitor reset halt
   (gdb) load
   (gdb) break main
   (gdb) continue

**Flash Programming**:

.. code-block:: bash

   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
       -c "program build-stm32f4/applications/blinky/blinky.elf verify reset exit"

J-Link
~~~~~~

SEGGER J-Link is a professional debugging probe.

Installation
^^^^^^^^^^^^

Download from https://www.segger.com/downloads/jlink/

**Linux**:

.. code-block:: bash

   # Download .deb package
   sudo dpkg -i JLink_Linux_*.deb

**macOS**:

.. code-block:: bash

   # Download .pkg installer
   # Install by double-clicking

**Windows**: Run installer

Configuration
^^^^^^^^^^^^^

Create ``jlink.cfg``:

.. code-block:: text

   [JLINK]
   Device = STM32F407VG
   Interface = SWD
   Speed = 4000

Usage
^^^^^

**J-Link GDB Server**:

.. code-block:: bash

   # Start GDB server
   JLinkGDBServer -device STM32F407VG -if SWD -speed 4000

   # Connect GDB
   arm-none-eabi-gdb build-stm32f4/applications/blinky/blinky.elf
   (gdb) target remote localhost:2331
   (gdb) monitor reset
   (gdb) load
   (gdb) break main
   (gdb) continue

**J-Link Commander**:

.. code-block:: bash

   # Start J-Link Commander
   JLinkExe -device STM32F407VG -if SWD -speed 4000

   # Commands
   J-Link> connect
   J-Link> loadfile blinky.elf
   J-Link> reset
   J-Link> go
   J-Link> halt
   J-Link> exit

Code Analysis Tools
-------------------

clang-format
~~~~~~~~~~~~

Automatic code formatting tool.

Installation
^^^^^^^^^^^^

**Linux**:

.. code-block:: bash

   sudo apt install clang-format

**macOS**:

.. code-block:: bash

   brew install clang-format

**Windows**: Included with LLVM

Usage
^^^^^

.. code-block:: bash

   # Format single file
   clang-format -i hal/src/hal_gpio.c

   # Format all files
   python scripts/tools/format.py

   # Check formatting without changes
   python scripts/tools/format.py --check

Configuration
^^^^^^^^^^^^^

Nexus uses ``.clang-format`` in project root:

.. code-block:: yaml

   BasedOnStyle: Google
   IndentWidth: 4
   ColumnLimit: 80
   PointerAlignment: Left
   AlignConsecutiveAssignments: false
   AlignConsecutiveDeclarations: false

clang-tidy
~~~~~~~~~~

Static analysis tool for C/C++.

Installation
^^^^^^^^^^^^

**Linux**:

.. code-block:: bash

   sudo apt install clang-tidy

**macOS**:

.. code-block:: bash

   brew install llvm

**Windows**: Included with LLVM

Usage
^^^^^

.. code-block:: bash

   # Analyze single file
   clang-tidy hal/src/hal_gpio.c -- -Ihal/include

   # Analyze all files
   find hal osal framework -name "*.c" | xargs clang-tidy

Configuration
^^^^^^^^^^^^^

Nexus uses ``.clang-tidy`` in project root:

.. code-block:: yaml

   Checks: >
     -*,
     bugprone-*,
     cert-*,
     clang-analyzer-*,
     cppcoreguidelines-*,
     modernize-*,
     performance-*,
     readability-*

cppcheck
~~~~~~~~

Static analysis tool focused on detecting bugs.

Installation
^^^^^^^^^^^^

**Linux**:

.. code-block:: bash

   sudo apt install cppcheck

**macOS**:

.. code-block:: bash

   brew install cppcheck

**Windows**: Download from http://cppcheck.sourceforge.net/

Usage
^^^^^

.. code-block:: bash

   # Analyze project
   cppcheck --enable=all --inconclusive --std=c11 \
       -I hal/include -I osal/include \
       hal/ osal/ framework/

   # Generate XML report
   cppcheck --enable=all --xml --xml-version=2 \
       hal/ osal/ framework/ 2> cppcheck_report.xml

Valgrind
~~~~~~~~

Memory debugging and profiling tool.

Installation
^^^^^^^^^^^^

**Linux**:

.. code-block:: bash

   sudo apt install valgrind

**macOS**:

.. code-block:: bash

   brew install valgrind

Usage
^^^^^

.. code-block:: bash

   # Memory leak detection
   valgrind --leak-check=full --show-leak-kinds=all \
       ./build/applications/blinky/blinky

   # Memory error detection
   valgrind --tool=memcheck --track-origins=yes \
       ./build/applications/blinky/blinky

   # Cache profiling
   valgrind --tool=cachegrind ./build/applications/blinky/blinky

AddressSanitizer
~~~~~~~~~~~~~~~~

Fast memory error detector.

Usage
^^^^^

.. code-block:: bash

   # Build with AddressSanitizer
   cmake -B build -DNEXUS_PLATFORM=native \
       -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
       -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
   cmake --build build

   # Run program
   ./build/applications/blinky/blinky

Documentation Tools
-------------------

Doxygen
~~~~~~~

API documentation generator.

Installation
^^^^^^^^^^^^

**Linux**:

.. code-block:: bash

   sudo apt install doxygen graphviz

**macOS**:

.. code-block:: bash

   brew install doxygen graphviz

**Windows**: Download from https://www.doxygen.nl/

Usage
^^^^^

.. code-block:: bash

   # Generate documentation
   doxygen Doxyfile

   # View documentation
   open docs/api/html/index.html

Configuration
^^^^^^^^^^^^^

Nexus uses ``Doxyfile`` in project root. Key settings:

.. code-block:: text

   PROJECT_NAME           = "Nexus"
   PROJECT_NUMBER         = 0.1.0
   OUTPUT_DIRECTORY       = docs/api
   INPUT                  = hal osal framework
   RECURSIVE              = YES
   EXTRACT_ALL            = YES
   GENERATE_HTML          = YES
   GENERATE_LATEX         = NO

Sphinx
~~~~~~

User documentation generator.

Installation
^^^^^^^^^^^^

.. code-block:: bash

   pip install sphinx sphinx_rtd_theme breathe

Usage
^^^^^

.. code-block:: bash

   # Generate documentation
   cd docs/sphinx
   sphinx-build -b html . _build/html

   # View documentation
   open _build/html/index.html

Configuration
^^^^^^^^^^^^^

Nexus uses ``docs/sphinx/conf.py``. Key settings:

.. code-block:: python

   project = 'Nexus'
   copyright = '2026, Nexus Team'
   author = 'Nexus Team'
   version = '0.1.0'
   release = '0.1.0'

   extensions = [
       'sphinx.ext.autodoc',
       'sphinx.ext.intersphinx',
       'sphinx.ext.todo',
       'sphinx.ext.viewcode',
       'breathe',
   ]

   html_theme = 'sphinx_rtd_theme'

Platform-Specific Setup
-----------------------

Windows Development
~~~~~~~~~~~~~~~~~~~

**Recommended Setup**:

* Windows 10/11
* Visual Studio 2019+ or MSVC Build Tools
* Windows Terminal
* WSL2 for Linux tools

**WSL2 Setup**:

.. code-block:: powershell

   # Install WSL2
   wsl --install

   # Install Ubuntu
   wsl --install -d Ubuntu

   # Inside WSL
   sudo apt update
   sudo apt install build-essential cmake git

Linux Development
~~~~~~~~~~~~~~~~~

**Recommended Distribution**: Ubuntu 20.04+ or Debian 11+

**Package Installation**:

.. code-block:: bash

   # Development tools
   sudo apt install build-essential cmake git

   # ARM toolchain
   sudo apt install gcc-arm-none-eabi

   # Debugging tools
   sudo apt install gdb openocd

   # Analysis tools
   sudo apt install clang-format clang-tidy cppcheck valgrind

   # Documentation tools
   sudo apt install doxygen graphviz

macOS Development
~~~~~~~~~~~~~~~~~

**Recommended Setup**:

* macOS 11+ (Big Sur or later)
* Xcode Command Line Tools
* Homebrew package manager

**Package Installation**:

.. code-block:: bash

   # Install Homebrew
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

   # Development tools
   brew install cmake git

   # ARM toolchain
   brew install --cask gcc-arm-embedded

   # Debugging tools
   brew install openocd

   # Analysis tools
   brew install clang-format llvm cppcheck

   # Documentation tools
   brew install doxygen graphviz

Environment Variables
---------------------

Recommended environment variables for Nexus development:

**Linux/macOS** (add to ``~/.bashrc`` or ``~/.zshrc``):

.. code-block:: bash

   # Nexus project root
   export NEXUS_ROOT=~/projects/nexus

   # ARM toolchain
   export ARM_TOOLCHAIN_PATH=/usr/bin

   # Build configuration
   export NEXUS_BUILD_DIR=build
   export NEXUS_PLATFORM=native

   # Python
   export PYTHONPATH=$NEXUS_ROOT/scripts:$PYTHONPATH

**Windows** (PowerShell profile):

.. code-block:: powershell

   # Nexus project root
   $env:NEXUS_ROOT = "C:\Projects\nexus"

   # ARM toolchain
   $env:ARM_TOOLCHAIN_PATH = "C:\Program Files (x86)\GNU Arm Embedded Toolchain\bin"

   # Build configuration
   $env:NEXUS_BUILD_DIR = "build"
   $env:NEXUS_PLATFORM = "native"

Shell Aliases
-------------

Useful shell aliases for Nexus development:

**Linux/macOS** (add to ``~/.bashrc`` or ``~/.zshrc``):

.. code-block:: bash

   # Navigation
   alias cdnx='cd $NEXUS_ROOT'

   # Build
   alias nxbuild='python $NEXUS_ROOT/scripts/building/build.py'
   alias nxclean='python $NEXUS_ROOT/scripts/tools/clean.py'

   # Test
   alias nxtest='python $NEXUS_ROOT/scripts/test/test.py'

   # Format
   alias nxfmt='python $NEXUS_ROOT/scripts/tools/format.py'

   # Documentation
   alias nxdocs='python $NEXUS_ROOT/scripts/tools/docs.py'

**Windows** (PowerShell profile):

.. code-block:: powershell

   # Navigation
   function cdnx { Set-Location $env:NEXUS_ROOT }

   # Build
   function nxbuild { python "$env:NEXUS_ROOT\scripts\building\build.py" $args }
   function nxclean { python "$env:NEXUS_ROOT\scripts\tools\clean.py" $args }

   # Test
   function nxtest { python "$env:NEXUS_ROOT\scripts\test\test.py" $args }

   # Format
   function nxfmt { python "$env:NEXUS_ROOT\scripts\tools\format.py" $args }

   # Documentation
   function nxdocs { python "$env:NEXUS_ROOT\scripts\tools\docs.py" $args }

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**IDE doesn't recognize includes**

Solution: Regenerate compile_commands.json:

.. code-block:: bash

   cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
   ln -s build/compile_commands.json .

**Debugger can't find source files**

Solution: Use absolute paths in debug configuration or set source path:

.. code-block:: text

   (gdb) set substitute-path /build/path /source/path

**OpenOCD connection fails**

Solution: Check permissions and udev rules:

.. code-block:: bash

   sudo usermod -a -G dialout $USER
   # Log out and log back in

**clang-format not found**

Solution: Install clang-format and add to PATH:

.. code-block:: bash

   sudo apt install clang-format
   which clang-format

Next Steps
----------

Now that your development environment is set up:

1. :doc:`coding_standards` - Learn the code style
2. :doc:`testing` - Write effective tests
3. :doc:`debugging_guide` - Master debugging techniques
4. :doc:`contributing` - Start contributing

See Also
--------

* :doc:`../getting_started/environment_setup` - Basic setup
* :doc:`debugging_guide` - Debugging techniques
* :doc:`scripts` - Build scripts
* :doc:`contributing` - Contribution guide
