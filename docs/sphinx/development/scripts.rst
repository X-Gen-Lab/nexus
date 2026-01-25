Build Scripts and Tools
=======================

The Nexus project includes a comprehensive set of Python scripts for building, testing, formatting, cleaning, and generating documentation. This guide covers all available scripts and their usage.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

All scripts are located in the ``scripts/`` directory and can be invoked directly or through the unified ``nexus.py`` entry point.

Script Organization
~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   scripts/
   ├── nexus.py                    # Unified entry point
   ├── nexus.sh                    # Bash wrapper
   ├── nexus.ps1                   # PowerShell wrapper
   ├── nexus.bat                   # Windows batch wrapper
   ├── setup/
   │   └── setup.py                # Environment setup
   ├── building/
   │   └── build.py                # Build script
   ├── test/
   │   └── test.py                 # Test runner
   ├── tools/
   │   ├── format.py               # Code formatter
   │   ├── clean.py                # Cleanup script
   │   └── docs.py                 # Documentation generator
   ├── ci/
   │   └── ci_build.py             # CI pipeline
   ├── Kconfig/
   │   ├── generate_config.py      # Config generator
   │   ├── validate_kconfig.py     # Config validator
   │   └── kconfig_migrate.py      # Config migration
   ├── coverage/
   │   ├── run_coverage_linux.sh   # Linux coverage
   │   └── run_coverage_windows.ps1 # Windows coverage
   └── validation/
       └── validate.py             # System validation

Unified Entry Point
-------------------

nexus.py
~~~~~~~~

The ``nexus.py`` script provides a unified interface to all project operations.

**Usage**:

.. code-block:: bash

   python nexus.py <command> [arguments...]

**Commands**:

* ``setup`` - Environment setup and configuration
* ``build`` - Build the project
* ``test`` - Run tests
* ``format`` - Format source code
* ``clean`` - Clean build artifacts
* ``docs`` - Generate documentation
* ``ci`` - Run CI pipeline
* ``help`` - Show help information

**Options**:

* ``--list`` - List all available commands
* ``--version`` - Show version information
* ``--help`` - Show help information

**Examples**:

.. code-block:: bash

   # Show version
   python nexus.py --version

   # List commands
   python nexus.py --list

   # Setup development environment
   python nexus.py setup --dev --docs

   # Build in Release mode
   python nexus.py build --type release

   # Run tests with verbose output
   python nexus.py test --verbose

   # Format code
   python nexus.py format --check

   # Clean all artifacts
   python nexus.py clean --all

   # Generate documentation
   python nexus.py docs --target all

   # Run CI pipeline
   python nexus.py ci --stage all --coverage

Platform-Specific Wrappers
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Bash (Linux/macOS)**:

.. code-block:: bash

   ./scripts/nexus.sh build --type release

**PowerShell (Windows)**:

.. code-block:: powershell

   .\scripts\nexus.ps1 build --type release

**Batch (Windows)**:

.. code-block:: batch

   scripts\nexus.bat build --type release

Build Scripts
-------------

build.py
~~~~~~~~

Located at ``scripts/building/build.py``, this script builds the project using CMake.

**Usage**:

.. code-block:: bash

   python scripts/building/build.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--type, -t``
     - Build type: debug, release, relwithdebinfo, minsizerel (default: debug)
   * - ``--platform, -p``
     - Target platform: native, stm32f4, stm32h7, ESP32, nrf52 (default: native)
   * - ``--osal``
     - OSAL backend: baremetal, FreeRTOS, rtthread, zephyr (default: baremetal)
   * - ``--build-dir``
     - Build directory (default: build-<type>)
   * - ``--clean``
     - Clean before building
   * - ``--tests``
     - Build tests (default: ON for native, OFF for embedded)
   * - ``--examples``
     - Build examples (default: ON)
   * - ``--docs``
     - Build documentation (default: OFF)
   * - ``--coverage``
     - Enable code coverage (default: OFF)
   * - ``--jobs, -j``
     - Number of parallel jobs (default: CPU count)
   * - ``--verbose, -v``
     - Verbose output
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Debug build for native platform
   python scripts/building/build.py

   # Release build for STM32F4
   python scripts/building/build.py --type release --platform stm32f4

   # Build with FreeRTOS
   python scripts/building/build.py --osal FreeRTOS

   # Clean build with tests and coverage
   python scripts/building/build.py --clean --tests --coverage

   # Parallel build with 8 jobs
   python scripts/building/build.py --jobs 8

   # Verbose build
   python scripts/building/build.py --verbose

Test Scripts
------------

test.py
~~~~~~~

Located at ``scripts/test/test.py``, this script runs unit tests using CTest and GoogleTest.

**Usage**:

.. code-block:: bash

   python scripts/test/test.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--filter, -f``
     - Test filter pattern (default: \*)
   * - ``--verbose, -v``
     - Verbose output
   * - ``--xml``
     - Generate XML report
   * - ``--build-dir``
     - Build directory (default: build-Debug)
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Run all tests
   python scripts/test/test.py

   # Run specific test suite
   python scripts/test/test.py --filter "ConfigTest.\*"

   # Run with verbose output
   python scripts/test/test.py --verbose

   # Generate XML report
   python scripts/test/test.py --xml test_results.xml

   # Use custom build directory
   python scripts/test/test.py --build-dir build-Release

**Test Filters**:

GoogleTest filter patterns:

* ``*`` - All tests
* ``TestSuite.*`` - All tests in TestSuite
* ``TestSuite.TestName`` - Specific test
* ``*Math*`` - All tests containing "Math"
* ``TestSuite.*:-TestSuite.SkipThis`` - All except SkipThis

Code Formatting
---------------

format.py
~~~~~~~~~

Located at ``scripts/tools/format.py``, this script formats C/C++ code using clang-format.

**Usage**:

.. code-block:: bash

   python scripts/tools/format.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--check``
     - Check formatting without modifying files
   * - ``--fix``
     - Fix formatting issues (default)
   * - ``--paths``
     - Specific paths to format (default: all)
   * - ``--verbose, -v``
     - Verbose output
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Format all code
   python scripts/tools/format.py

   # Check formatting without changes
   python scripts/tools/format.py --check

   # Format specific directory
   python scripts/tools/format.py --paths hal/

   # Format multiple paths
   python scripts/tools/format.py --paths hal/ osal/ framework/

**Configuration**:

Formatting rules are defined in ``.clang-format`` at the project root.

Cleanup Scripts
---------------

clean.py
~~~~~~~~

Located at ``scripts/tools/clean.py``, this script cleans build artifacts.

**Usage**:

.. code-block:: bash

   python scripts/tools/clean.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--all``
     - Clean all build directories
   * - ``--build-dir``
     - Specific build directory to clean
   * - ``--docs``
     - Clean documentation artifacts
   * - ``--coverage``
     - Clean coverage data
   * - ``--cache``
     - Clean CMake cache
   * - ``--verbose, -v``
     - Verbose output
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Clean default build directory
   python scripts/tools/clean.py

   # Clean all build directories
   python scripts/tools/clean.py --all

   # Clean specific build directory
   python scripts/tools/clean.py --build-dir build-Release

   # Clean documentation
   python scripts/tools/clean.py --docs

   # Clean coverage data
   python scripts/tools/clean.py --coverage

   # Clean everything
   python scripts/tools/clean.py --all --docs --coverage

Documentation Scripts
---------------------

docs.py
~~~~~~~

Located at ``scripts/tools/docs.py``, this script generates documentation using Doxygen and Sphinx.

**Usage**:

.. code-block:: bash

   python scripts/tools/docs.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--target, -t``
     - Target: doxygen, sphinx, or all (default: all)
   * - ``--verbose, -v``
     - Verbose output
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Generate all documentation
   python scripts/tools/docs.py

   # Generate only Doxygen API docs
   python scripts/tools/docs.py --target doxygen

   # Generate only Sphinx user docs
   python scripts/tools/docs.py --target sphinx

   # Verbose output
   python scripts/tools/docs.py --verbose

**Output Locations**:

* Doxygen: ``docs/api/html/index.html``
* Sphinx: ``docs/sphinx/_build/html/index.html``

Kconfig Tools
-------------

generate_config.py
~~~~~~~~~~~~~~~~~~

Located at ``scripts/Kconfig/generate_config.py``, this script generates configuration headers from Kconfig.

**Usage**:

.. code-block:: bash

   python scripts/Kconfig/generate_config.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--config``
     - Input .config file
   * - ``--output``
     - Output header file (default: nexus_config.h)
   * - ``--default``
     - Generate default configuration
   * - ``--menuconfig``
     - Launch interactive menuconfig
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Generate from .config
   python scripts/Kconfig/generate_config.py --config .config --output nexus_config.h

   # Generate default configuration
   python scripts/Kconfig/generate_config.py --default --output nexus_config.h

   # Launch menuconfig
   python scripts/Kconfig/generate_config.py --menuconfig

validate_kconfig.py
~~~~~~~~~~~~~~~~~~~

Located at ``scripts/Kconfig/validate_kconfig.py``, this script validates Kconfig files and configurations.

**Usage**:

.. code-block:: bash

   python scripts/Kconfig/validate_kconfig.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--config``
     - Configuration file to validate
   * - ``--Kconfig``
     - Kconfig file to validate (default: Kconfig)
   * - ``--strict``
     - Strict validation mode
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Validate .config
   python scripts/Kconfig/validate_kconfig.py --config .config

   # Validate Kconfig structure
   python scripts/Kconfig/validate_kconfig.py --Kconfig Kconfig

   # Strict validation
   python scripts/Kconfig/validate_kconfig.py --config .config --strict

kconfig_migrate.py
~~~~~~~~~~~~~~~~~~

Located at ``scripts/Kconfig/migrate_kconfig.py``, this script migrates configurations between versions.

**Usage**:

.. code-block:: bash

   python scripts/Kconfig/kconfig_migrate.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--old-config``
     - Old configuration file
   * - ``--new-config``
     - New configuration file
   * - ``--Kconfig``
     - New Kconfig file
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Migrate configuration
   python scripts/Kconfig/kconfig_migrate.py --old-config .config.old --new-config .config

Coverage Scripts
----------------

run_coverage_linux.sh
~~~~~~~~~~~~~~~~~~~~~

Located at ``scripts/coverage/run_coverage_linux.sh``, this script generates code coverage reports on Linux.

**Usage**:

.. code-block:: bash

   bash scripts/coverage/run_coverage_linux.sh

**Output**:

* Coverage data: ``coverage.info``
* HTML report: ``coverage_html/index.html``

run_coverage_windows.ps1
~~~~~~~~~~~~~~~~~~~~~~~~

Located at ``scripts/coverage/run_coverage_windows.ps1``, this script generates code coverage reports on Windows.

**Usage**:

.. code-block:: powershell

   .\scripts\coverage\run_coverage_windows.ps1

**Requirements**:

* OpenCppCoverage or Visual Studio Code Coverage tools

CI Scripts
----------

ci_build.py
~~~~~~~~~~~

Located at ``scripts/ci/ci_build.py``, this script runs the CI pipeline.

**Usage**:

.. code-block:: bash

   python scripts/ci/ci_build.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--stage``
     - CI stage: build, test, coverage, docs, all (default: all)
   * - ``--platform``
     - Target platform (default: native)
   * - ``--coverage``
     - Enable coverage reporting
   * - ``--verbose, -v``
     - Verbose output
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Run full CI pipeline
   python scripts/ci/ci_build.py --stage all

   # Run only build stage
   python scripts/ci/ci_build.py --stage build

   # Run with coverage
   python scripts/ci/ci_build.py --stage all --coverage

   # CI for STM32F4
   python scripts/ci/ci_build.py --platform stm32f4

Setup Scripts
-------------

setup.py
~~~~~~~~

Located at ``scripts/setup/setup.py``, this script sets up the development environment.

**Usage**:

.. code-block:: bash

   python scripts/setup/setup.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--dev``
     - Install development dependencies
   * - ``--docs``
     - Install documentation dependencies
   * - ``--test``
     - Install testing dependencies
   * - ``--all``
     - Install all dependencies
   * - ``--check``
     - Check environment without installing
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Setup development environment
   python scripts/setup/setup.py --dev

   # Setup for documentation
   python scripts/setup/setup.py --docs

   # Setup everything
   python scripts/setup/setup.py --all

   # Check environment
   python scripts/setup/setup.py --check

Validation Scripts
------------------

validate.py
~~~~~~~~~~~

Located at ``scripts/validation/validate.py``, this script validates the system configuration and tests.

**Usage**:

.. code-block:: bash

   python scripts/validation/validate.py [options]

**Options**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Option
     - Description
   * - ``--platform``
     - Platform to validate (default: all)
   * - ``--coverage``
     - Include coverage analysis
   * - ``--report``
     - Generate validation report
   * - ``--help, -h``
     - Show help message

**Examples**:

.. code-block:: bash

   # Validate all platforms
   python scripts/validation/validate.py

   # Validate specific platform
   python scripts/validation/validate.py --platform native

   # Validate with coverage
   python scripts/validation/validate.py --coverage

   # Generate report
   python scripts/validation/validate.py --report

Common Workflows
----------------

Development Workflow
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # 1. Setup environment
   python nexus.py setup --dev

   # 2. Build in debug mode
   python nexus.py build --type debug

   # 3. Run tests
   python nexus.py test --verbose

   # 4. Format code
   python nexus.py format

   # 5. Clean artifacts
   python nexus.py clean

Release Workflow
~~~~~~~~~~~~~~~~

.. code-block:: bash

   # 1. Clean everything
   python nexus.py clean --all

   # 2. Build release for all platforms
   python nexus.py build --type release --platform native
   python nexus.py build --type release --platform stm32f4
   python nexus.py build --type release --platform stm32h7

   # 3. Run tests
   python nexus.py test

   # 4. Generate documentation
   python nexus.py docs --target all

CI/CD Workflow
~~~~~~~~~~~~~~

.. code-block:: bash

   # Run full CI pipeline with coverage
   python nexus.py ci --stage all --coverage

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**Script not found**

.. code-block:: text

   FileNotFoundError: [Errno 2] No such file or directory: 'scripts/...'

Solution: Ensure you're running scripts from the project root directory.

**Python version too old**

.. code-block:: text

   Python 3.7 or higher is required

Solution: Upgrade Python to version 3.7 or higher.

**Missing dependencies**

.. code-block:: text

   ModuleNotFoundError: No module named 'kconfiglib'

Solution: Install dependencies:

.. code-block:: bash

   pip install -r requirements.txt

**Permission denied (Linux/macOS)**

.. code-block:: text

   Permission denied: './scripts/nexus.sh'

Solution: Make scripts executable:

.. code-block:: bash

   chmod +x scripts/\*.sh

See Also
--------

* :doc:`../user_guide/build_system` - Build system documentation
* :doc:`testing` - Testing guide
* :doc:`contributing` - Contribution guidelines
* :doc:`kconfig_guide` - Kconfig configuration guide
