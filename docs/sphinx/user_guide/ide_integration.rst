IDE Integration
===============

This guide covers integrating the Nexus Embedded Platform with popular IDEs including Visual Studio Code, CLion, and Visual Studio.

.. contents:: Table of Contents
   :local:
   :depth: 2

Visual Studio Code
------------------

VS Code is a lightweight, cross-platform editor with excellent CMake and C/C++ support.

Prerequisites
~~~~~~~~~~~~~

Install the following VS Code extensions:

* **C/C++** (ms-vscode.cpptools) - IntelliSense, debugging
* **CMake Tools** (ms-vscode.CMake-tools) - CMake integration
* **CMake** (twxs.CMake) - CMake language support

Installation:

.. code-block:: bash

   code --install-extension ms-vscode.cpptools
   code --install-extension ms-vscode.CMake-tools
   code --install-extension twxs.CMake

Configuration
~~~~~~~~~~~~~

Create ``.vscode/settings.json``:

.. code-block:: json

   {
       "CMake.configureOnOpen": true,
       "CMake.buildDirectory": "${workspaceFolder}/build",
       "CMake.generator": "Ninja",
       "CMake.configureSettings": {
           "NEXUS_PLATFORM": "native",
           "NEXUS_BUILD_TESTS": "ON",
           "NEXUS_BUILD_EXAMPLES": "ON",
           "CMAKE_BUILD_TYPE": "Debug",
           "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
       },
       "C_Cpp.default.configurationProvider": "ms-vscode.CMake-tools",
       "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json",
       "files.associations": {
           "*.h": "c",
           "*.c": "c"
       }
   }

Create ``.vscode/c_cpp_properties.json``:

.. code-block:: json

   {
       "configurations": [
           {
               "name": "Linux",
               "includePath": [
                   "${workspaceFolder}/**"
               ],
               "defines": [],
               "compilerPath": "/usr/bin/gcc",
               "cStandard": "c11",
               "cppStandard": "c++17",
               "intelliSenseMode": "linux-gcc-x64",
               "compileCommands": "${workspaceFolder}/build/compile_commands.json"
           },
           {
               "name": "Win32",
               "includePath": [
                   "${workspaceFolder}/**"
               ],
               "defines": [
                   "_DEBUG",
                   "UNICODE",
                   "_UNICODE"
               ],
               "windowsSdkVersion": "10.0.19041.0",
               "compilerPath": "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.30.30705/bin/Hostx64/x64/cl.exe",
               "cStandard": "c11",
               "cppStandard": "c++17",
               "intelliSenseMode": "windows-msvc-x64",
               "compileCommands": "${workspaceFolder}/build/compile_commands.json"
           },
           {
               "name": "Mac",
               "includePath": [
                   "${workspaceFolder}/**"
               ],
               "defines": [],
               "macFrameworkPath": [
                   "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks"
               ],
               "compilerPath": "/usr/bin/clang",
               "cStandard": "c11",
               "cppStandard": "c++17",
               "intelliSenseMode": "macos-clang-x64",
               "compileCommands": "${workspaceFolder}/build/compile_commands.json"
           }
       ],
       "version": 4
   }

Create ``.vscode/launch.json`` for debugging:

.. code-block:: json

   {
       "version": "0.2.0",
       "configurations": [
           {
               "name": "(gdb) Launch Tests",
               "type": "cppdbg",
               "request": "launch",
               "program": "${workspaceFolder}/build/tests/nexus_tests",
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
               ],
               "preLaunchTask": "CMake: build",
               "miDebuggerPath": "/usr/bin/gdb",
               "linux": {
                   "MIMode": "gdb",
                   "miDebuggerPath": "/usr/bin/gdb"
               },
               "osx": {
                   "MIMode": "lldb"
               },
               "windows": {
                   "MIMode": "gdb",
                   "miDebuggerPath": "C:\\msys64\\mingw64\\bin\\gdb.exe"
               }
           },
           {
               "name": "(gdb) Launch Blinky",
               "type": "cppdbg",
               "request": "launch",
               "program": "${workspaceFolder}/build/applications/blinky/blinky",
               "args": [],
               "stopAtEntry": false,
               "cwd": "${workspaceFolder}",
               "environment": [],
               "externalConsole": false,
               "MIMode": "gdb",
               "preLaunchTask": "CMake: build"
           }
       ]
   }

Create ``.vscode/tasks.json``:

.. code-block:: json

   {
       "version": "2.0.0",
       "tasks": [
           {
               "label": "CMake: configure",
               "type": "shell",
               "command": "CMake",
               "args": [
                   "-B",
                   "${workspaceFolder}/build",
                   "-DNEXUS_PLATFORM=native",
                   "-DCMAKE_BUILD_TYPE=Debug"
               ],
               "group": "build",
               "problemMatcher": []
           },
           {
               "label": "CMake: build",
               "type": "shell",
               "command": "CMake",
               "args": [
                   "--build",
                   "${workspaceFolder}/build",
                   "--",
                   "-j4"
               ],
               "group": {
                   "kind": "build",
                   "isDefault": true
               },
               "problemMatcher": ["$gcc"],
               "dependsOn": ["CMake: configure"]
           },
           {
               "label": "CMake: clean",
               "type": "shell",
               "command": "CMake",
               "args": [
                   "--build",
                   "${workspaceFolder}/build",
                   "--target",
                   "clean"
               ],
               "group": "build",
               "problemMatcher": []
           },
           {
               "label": "Run Tests",
               "type": "shell",
               "command": "ctest",
               "args": [
                   "--test-dir",
                   "${workspaceFolder}/build",
                   "--output-on-failure"
               ],
               "group": "test",
               "problemMatcher": [],
               "dependsOn": ["CMake: build"]
           }
       ]
   }

Usage
~~~~~

1. **Open Project**: ``File > Open Folder`` and select the Nexus root directory

2. **Configure**: Press ``Ctrl+Shift+P`` and run ``CMake: Configure``

3. **Build**: Press ``Ctrl+Shift+P`` and run ``CMake: Build`` or press ``F7``

4. **Run Tests**: Press ``Ctrl+Shift+P`` and run ``CMake: Run Tests``

5. **Debug**: Press ``F5`` to start debugging

Keyboard Shortcuts
~~~~~~~~~~~~~~~~~~

* ``F7`` - Build
* ``F5`` - Start debugging
* ``Shift+F5`` - Stop debugging
* ``F9`` - Toggle breakpoint
* ``F10`` - Step over
* ``F11`` - Step into
* ``Shift+F11`` - Step out

CLion
-----

CLion is a cross-platform C/C++ IDE from JetBrains with native CMake support.

Prerequisites
~~~~~~~~~~~~~

CLion automatically detects CMake projects. Ensure you have:

* CLion 2020.3 or later
* CMake 3.16 or later
* C/C++ compiler (GCC, Clang, or MSVC)
* ARM GCC toolchain (for embedded targets)

Configuration
~~~~~~~~~~~~~

CLion automatically detects CMake projects. No additional configuration is required for basic usage.

Custom CMake Options
~~~~~~~~~~~~~~~~~~~~

1. Open ``File > Settings > Build, Execution, Deployment > CMake``

2. Add build profiles:

**Debug Profile**:

* Name: ``Debug``
* Build type: ``Debug``
* CMake options: ``-DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON``
* Build directory: ``build-Debug``

**Release Profile**:

* Name: ``Release``
* Build type: ``Release``
* CMake options: ``-DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=OFF``
* Build directory: ``build-Release``

**STM32F4 Profile**:

* Name: ``STM32F4``
* Build type: ``Release``
* CMake options: ``-DNEXUS_PLATFORM=stm32f4 -DNEXUS_BUILD_TESTS=OFF``
* Build directory: ``build-stm32f4``
* Toolchain: ``ARM GCC`` (configure in ``File > Settings > Build, Execution, Deployment > Toolchains``)

Toolchain Configuration
~~~~~~~~~~~~~~~~~~~~~~~

For ARM cross-compilation:

1. Open ``File > Settings > Build, Execution, Deployment > Toolchains``

2. Add new toolchain:

* Name: ``ARM GCC``
* C Compiler: ``/usr/bin/arm-none-eabi-gcc`` (or Windows path)
* C++ Compiler: ``/usr/bin/arm-none-eabi-g++``
* Debugger: ``/usr/bin/arm-none-eabi-gdb``

Run/Debug Configurations
~~~~~~~~~~~~~~~~~~~~~~~~

CLion automatically creates run configurations for executables.

To create a custom configuration:

1. ``Run > Edit Configurations``

2. Add new ``CMake Application``:

* Name: ``Nexus Tests``
* Target: ``nexus_tests``
* Executable: ``nexus_tests``
* Program arguments: ``--gtest_filter=*``

Usage
~~~~~

1. **Open Project**: ``File > Open`` and select ``CMakeLists.txt``

2. **Reload CMake**: ``Tools > CMake > Reload CMake Project``

3. **Build**: ``Build > Build Project`` or ``Ctrl+F9``

4. **Run**: ``Run > Run`` or ``Shift+F10``

5. **Debug**: ``Run > Debug`` or ``Shift+F9``

6. **Run Tests**: Right-click on ``tests`` folder and select ``Run Tests``

Keyboard Shortcuts
~~~~~~~~~~~~~~~~~~

* ``Ctrl+F9`` - Build project
* ``Shift+F10`` - Run
* ``Shift+F9`` - Debug
* ``Ctrl+F8`` - Toggle breakpoint
* ``F8`` - Step over
* ``F7`` - Step into
* ``Shift+F8`` - Step out

Visual Studio
-------------

Visual Studio 2019/2022 has native CMake support.

Prerequisites
~~~~~~~~~~~~~

Install Visual Studio with:

* **Desktop development with C++** workload
* **C++ CMake tools for Windows** component
* **C++ Clang tools for Windows** (optional)

Configuration
~~~~~~~~~~~~~

Visual Studio uses ``CMakeSettings.json`` for configuration.

Create ``CMakeSettings.json``:

.. code-block:: json

   {
       "configurations": [
           {
               "name": "x64-Debug",
               "generator": "Ninja",
               "configurationType": "Debug",
               "inheritEnvironments": [ "msvc_x64_x64" ],
               "buildRoot": "${projectDir}\\build\\${name}",
               "installRoot": "${projectDir}\\install\\${name}",
               "cmakeCommandArgs": "-DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON",
               "buildCommandArgs": "",
               "ctestCommandArgs": "",
               "variables": [
                   {
                       "name": "CMAKE_BUILD_TYPE",
                       "value": "Debug",
                       "type": "STRING"
                   },
                   {
                       "name": "NEXUS_PLATFORM",
                       "value": "native",
                       "type": "STRING"
                   },
                   {
                       "name": "NEXUS_BUILD_TESTS",
                       "value": "ON",
                       "type": "BOOL"
                   }
               ]
           },
           {
               "name": "x64-Release",
               "generator": "Ninja",
               "configurationType": "Release",
               "inheritEnvironments": [ "msvc_x64_x64" ],
               "buildRoot": "${projectDir}\\build\\${name}",
               "installRoot": "${projectDir}\\install\\${name}",
               "cmakeCommandArgs": "-DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=OFF",
               "buildCommandArgs": "",
               "ctestCommandArgs": "",
               "variables": [
                   {
                       "name": "CMAKE_BUILD_TYPE",
                       "value": "Release",
                       "type": "STRING"
                   }
               ]
           }
       ]
   }

Usage
~~~~~

1. **Open Project**: ``File > Open > CMake`` and select ``CMakeLists.txt``

2. **Select Configuration**: Use the configuration dropdown in the toolbar

3. **Build**: ``Build > Build All`` or ``Ctrl+Shift+B``

4. **Run Tests**: ``Test > Run All Tests``

5. **Debug**: Set breakpoints and press ``F5``

Test Explorer
~~~~~~~~~~~~~

Visual Studio integrates with Google Test:

1. Open ``Test > Test Explorer``

2. Tests appear automatically after building

3. Run individual tests or test suites

4. View test output and failures

Keyboard Shortcuts
~~~~~~~~~~~~~~~~~~

* ``Ctrl+Shift+B`` - Build solution
* ``F5`` - Start debugging
* ``Ctrl+F5`` - Start without debugging
* ``Shift+F5`` - Stop debugging
* ``F9`` - Toggle breakpoint
* ``F10`` - Step over
* ``F11`` - Step into
* ``Shift+F11`` - Step out

ARM Cross-Compilation in Visual Studio
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For ARM targets, use Visual Studio with external toolchain:

1. Install ARM GCC toolchain

2. Create ARM configuration in ``CMakeSettings.json``:

.. code-block:: json

   {
       "name": "ARM-STM32F4",
       "generator": "Ninja",
       "configurationType": "Release",
       "buildRoot": "${projectDir}\\build\\${name}",
       "cmakeCommandArgs": "-DNEXUS_PLATFORM=stm32f4 -DCMAKE_TOOLCHAIN_FILE=CMake/toolchains/arm-none-eabi.CMake",
       "variables": [
           {
               "name": "TOOLCHAIN_PREFIX",
               "value": "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2021.10",
               "type": "PATH"
           }
       ]
   }

3. Build using this configuration

Common Issues
-------------

VS Code
~~~~~~~

**IntelliSense not working**

Solution: Ensure ``compile_commands.json`` is generated:

.. code-block:: bash

   CMake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

**CMake Tools extension not detecting project**

Solution: Reload window (``Ctrl+Shift+P`` > ``Developer: Reload Window``)

CLion
~~~~~

**CMake errors on project open**

Solution: Check CMake version and ensure all dependencies are installed.

**Toolchain not found**

Solution: Configure toolchain in ``File > Settings > Build, Execution, Deployment > Toolchains``

Visual Studio
~~~~~~~~~~~~~

**CMake configuration fails**

Solution: Check ``Output > CMake`` window for errors. Ensure CMake 3.16+ is installed.

**Tests not appearing in Test Explorer**

Solution: Rebuild project and refresh Test Explorer.

See Also
--------

* :doc:`build_system` - Build system documentation
* :doc:`../development/testing` - Testing guide
* :doc:`../getting_started/environment_setup` - Installation guide
