Kconfig Tools Reference
=======================

This guide provides detailed documentation for all Kconfig configuration tools in the Nexus Embedded Platform.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

The Nexus platform provides several Python-based tools for working with Kconfig configurations:

* **generate_config.py**: Generate C header files from .config
* **validate_kconfig.py**: Validate Kconfig syntax and dependencies
* **kconfig_migrate.py**: Migrate configurations between versions
* **kconfig_diff.py**: Compare two configurations
* **generate_config_docs.py**: Generate configuration documentation

All tools are located in ``scripts/Kconfig/`` directory.


generate_config.py
------------------

Overview
^^^^^^^^

Generates C header files from Kconfig ``.config`` files. This is the primary tool for converting Kconfig configuration to C macros.

Usage
^^^^^

**Basic usage:**

.. code-block:: bash

    python scripts/Kconfig/generate_config.py

**With options:**

.. code-block:: bash

    python scripts/Kconfig/generate_config.py \
        --config .config \
        --output nexus_config.h \
        --prefix NX_CONFIG_

Command-Line Options
^^^^^^^^^^^^^^^^^^^^

.. code-block:: text

    --config PATH          Path to .config file (default: .config)
    --output PATH          Output header file path (default: nexus_config.h)
    --prefix PREFIX        Macro prefix (default: NX_CONFIG_)
    --Kconfig PATH         Root Kconfig file (default: Kconfig)
    --default              Generate default configuration
    --verbose              Enable verbose output
    --help                 Show help message

Examples
^^^^^^^^

**Generate from .config:**

.. code-block:: bash

    python scripts/Kconfig/generate_config.py --config .config

**Generate default configuration:**

.. code-block:: bash

    python scripts/Kconfig/generate_config.py --default

**Custom output file:**

.. code-block:: bash

    python scripts/Kconfig/generate_config.py \
        --config platforms/native/defconfig \
        --output build/native_config.h

**Custom prefix:**

.. code-block:: bash

    python scripts/Kconfig/generate_config.py \
        --prefix MY_CONFIG_

Output Format
^^^^^^^^^^^^^

**Input (.config):**

.. code-block:: Kconfig

    CONFIG_PLATFORM_STM32=y
    CONFIG_STM32F407=y
    CONFIG_UART1_BAUDRATE=115200
    CONFIG_OSAL_FREERTOS=y

**Output (nexus_config.h):**

.. code-block:: c

    #ifndef NEXUS_CONFIG_H
    #define NEXUS_CONFIG_H

    #define NX_CONFIG_PLATFORM_STM32 1
    #define NX_CONFIG_STM32F407 1
    #define NX_CONFIG_UART1_BAUDRATE 115200
    #define NX_CONFIG_OSAL_FREERTOS 1

    #endif /* NEXUS_CONFIG_H */

Type Conversion
^^^^^^^^^^^^^^^

**Boolean values:**

.. code-block:: Kconfig

    CONFIG_UART_ENABLE=y    # -> #define NX_CONFIG_UART_ENABLE 1
    CONFIG_ADC_ENABLE=n     # -> /* #undef NX_CONFIG_ADC_ENABLE */

**Integer values:**

.. code-block:: Kconfig

    CONFIG_UART1_BAUDRATE=115200  # -> #define NX_CONFIG_UART1_BAUDRATE 115200

**Hexadecimal values:**

.. code-block:: Kconfig

    CONFIG_LINKER_RAM_START=0x20000000  # -> #define NX_CONFIG_LINKER_RAM_START 0x20000000

**String values:**

.. code-block:: Kconfig

    CONFIG_PLATFORM_NAME="STM32"  # -> #define NX_CONFIG_PLATFORM_NAME "STM32"

Integration with Build System
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**CMake integration:**

.. code-block:: CMake

    # Generate configuration header
    execute_process(
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/scripts/Kconfig/generate_config.py
                --config ${CMAKE_SOURCE_DIR}/.config
                --output ${CMAKE_SOURCE_DIR}/nexus_config.h
        RESULT_VARIABLE RESULT
    )

    if(NOT RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to generate configuration")
    endif()

**Make integration:**

.. code-block:: makefile

    nexus_config.h: .config
    	python scripts/Kconfig/generate_config.py

Error Handling
^^^^^^^^^^^^^^

**Missing .config file:**

.. code-block:: text

    Error: Configuration file '.config' not found
    Use --default to generate default configuration

**Invalid Kconfig syntax:**

.. code-block:: text

    Error: Invalid Kconfig syntax in platforms/STM32/Kconfig:42
    Unexpected token: 'endmenu'

**Circular dependency:**

.. code-block:: text

    Error: Circular dependency detected:
    CONFIG_A depends on CONFIG_B
    CONFIG_B depends on CONFIG_A


validate_kconfig.py
-------------------

Overview
^^^^^^^^

Validates Kconfig files for syntax errors, dependency issues, and structural problems.

Usage
^^^^^

**Basic usage:**

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py Kconfig

**With options:**

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py Kconfig \
        --check-deps \
        --check-values \
        --config .config

Command-Line Options
^^^^^^^^^^^^^^^^^^^^

.. code-block:: text

    KCONFIG_FILE           Root Kconfig file to validate
    --check-deps           Check for dependency issues
    --check-values         Validate configuration values
    --config PATH          Configuration file to validate
    --verbose              Enable verbose output
    --help                 Show help message

Examples
^^^^^^^^

**Validate Kconfig syntax:**

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py Kconfig

**Check dependencies:**

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py Kconfig --check-deps

**Validate configuration values:**

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py Kconfig \
        --check-values \
        --config .config

**Validate specific file:**

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py platforms/STM32/Kconfig

Validation Checks
^^^^^^^^^^^^^^^^^

**Syntax validation:**

* Block structure (if/endif, menu/endmenu, choice/endchoice)
* Keyword spelling
* Indentation consistency
* Comment syntax

**Dependency validation:**

* Circular dependencies
* Undefined symbol references
* Unsatisfiable dependencies
* Missing dependencies

**Value validation:**

* Range constraint violations
* Invalid type values
* Missing required symbols
* Conflicting values

**Structural validation:**

* Source file existence
* Duplicate symbol definitions
* Orphaned symbols
* Unused symbols

Output Format
^^^^^^^^^^^^^

**Success:**

.. code-block:: text

    Validating Kconfig...
    ✓ Syntax validation passed
    ✓ Dependency validation passed
    ✓ Value validation passed

    Validation completed successfully

**Errors:**

.. code-block:: text

    Validating Kconfig...
    ✗ Syntax errors found:
      platforms/STM32/Kconfig:42: Unexpected 'endmenu' without matching 'menu'
      platforms/STM32/Kconfig:58: Missing 'endif' for 'if' at line 45

    ✗ Dependency errors found:
      CONFIG_UART1_DMA_ENABLE depends on undefined symbol CONFIG_DMA_ENABLE
      Circular dependency: CONFIG_A -> CONFIG_B -> CONFIG_A

    ✗ Value errors found:
      CONFIG_UART1_BAUDRATE=1000000 violates range [9600, 921600]
      CONFIG_PLATFORM_STM32 and CONFIG_PLATFORM_GD32 cannot both be set

    Validation failed with 6 errors

Integration with CI/CD
^^^^^^^^^^^^^^^^^^^^^^

**GitHub Actions:**

.. code-block:: yaml

    - name: Validate Kconfig
      run: |
        python scripts/Kconfig/validate_kconfig.py Kconfig
        if [ $? -ne 0 ]; then
          echo "Kconfig validation failed"
          exit 1
        fi

**Pre-commit hook:**

.. code-block:: bash

    #!/bin/bash
    # .git/hooks/pre-commit

    python scripts/Kconfig/validate_kconfig.py Kconfig
    if [ $? -ne 0 ]; then
        echo "Kconfig validation failed. Commit aborted."
        exit 1
    fi


kconfig_migrate.py
------------------

Overview
^^^^^^^^

Migrates configuration files between different versions of the Nexus platform.

Usage
^^^^^

**Basic usage:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_migrate.py \
        --input old.config \
        --output new.config \
        --version 2.0

**With migration rules:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_migrate.py \
        --input old.config \
        --output new.config \
        --rules migration_rules.py

Command-Line Options
^^^^^^^^^^^^^^^^^^^^

.. code-block:: text

    --input PATH           Input configuration file
    --output PATH          Output configuration file
    --version VERSION      Target version
    --rules PATH           Migration rules file
    --dry-run              Show changes without writing
    --verbose              Enable verbose output
    --help                 Show help message

Examples
^^^^^^^^

**Migrate to version 2.0:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_migrate.py \
        --input .config \
        --output .config.new \
        --version 2.0

**Dry run (preview changes):**

.. code-block:: bash

    python scripts/Kconfig/kconfig_migrate.py \
        --input .config \
        --output .config.new \
        --version 2.0 \
        --dry-run

**Custom migration rules:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_migrate.py \
        --input .config \
        --output .config.new \
        --rules custom_rules.py

Migration Rules
^^^^^^^^^^^^^^^

**Symbol renaming:**

.. code-block:: python

    # migration_rules.py
    SYMBOL_RENAMES = {
        'OLD_UART_ENABLE': 'STM32_UART_ENABLE',
        'OLD_BAUDRATE': 'UART1_BAUDRATE',
        'OLD_GPIO_ENABLE': 'NATIVE_GPIO_ENABLE',
    }

**Symbol removal:**

.. code-block:: python

    DEPRECATED_SYMBOLS = [
        'OLD_FEATURE_ENABLE',
        'LEGACY_MODE',
    ]

**Value transformation:**

.. code-block:: python

    VALUE_TRANSFORMS = {
        'UART1_MODE': {
            0: 'UART1_MODE_POLLING',
            1: 'UART1_MODE_INTERRUPT',
            2: 'UART1_MODE_DMA',
        }
    }

**Default values:**

.. code-block:: python

    NEW_DEFAULTS = {
        'NEW_FEATURE_ENABLE': 'y',
        'NEW_BUFFER_SIZE': '256',
    }

Output Format
^^^^^^^^^^^^^

**Migration log:**

.. code-block:: text

    Migrating configuration from v1.0 to v2.0...

    Renamed symbols:
      OLD_UART_ENABLE -> STM32_UART_ENABLE
      OLD_BAUDRATE -> UART1_BAUDRATE

    Removed deprecated symbols:
      OLD_FEATURE_ENABLE
      LEGACY_MODE

    Added new symbols with defaults:
      NEW_FEATURE_ENABLE=y
      NEW_BUFFER_SIZE=256

    Transformed values:
      UART1_MODE: 2 -> UART1_MODE_DMA

    Migration completed successfully
    Backup saved to: .config.backup

**Dry run output:**

.. code-block:: text

    [DRY RUN] Would rename:
      OLD_UART_ENABLE -> STM32_UART_ENABLE

    [DRY RUN] Would remove:
      OLD_FEATURE_ENABLE

    [DRY RUN] Would add:
      NEW_FEATURE_ENABLE=y

    No changes written (dry run mode)

Best Practices
^^^^^^^^^^^^^^

* Always backup configuration before migration
* Use dry-run to preview changes
* Test migrated configuration
* Validate after migration
* Document migration rules


kconfig_diff.py
---------------

Overview
^^^^^^^^

Compares two configuration files and shows differences.

Usage
^^^^^

**Basic usage:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py old.config new.config

**With options:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py old.config new.config \
        --format json \
        --output diff.json

Command-Line Options
^^^^^^^^^^^^^^^^^^^^

.. code-block:: text

    CONFIG1                First configuration file
    CONFIG2                Second configuration file
    --format FORMAT        Output format (text, json, html)
    --output PATH          Output file path
    --ignore-comments      Ignore comment differences
    --verbose              Enable verbose output
    --help                 Show help message

Examples
^^^^^^^^

**Text format (default):**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py .config platforms/native/defconfig

**JSON format:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py old.config new.config \
        --format json \
        --output diff.json

**HTML format:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py old.config new.config \
        --format html \
        --output diff.html

**Ignore comments:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py old.config new.config \
        --ignore-comments

Output Formats
^^^^^^^^^^^^^^

**Text format:**

.. code-block:: text

    Configuration Diff: old.config vs new.config

    Added symbols (3):
      + CONFIG_NEW_FEATURE=y
      + CONFIG_NEW_BUFFER_SIZE=256
      + CONFIG_NEW_MODE=2

    Removed symbols (2):
      - CONFIG_OLD_FEATURE=y
      - CONFIG_LEGACY_MODE=1

    Changed symbols (4):
      ~ CONFIG_UART1_BAUDRATE: 9600 -> 115200
      ~ CONFIG_HEAP_SIZE: 16384 -> 32768
      ~ CONFIG_PLATFORM_NAME: "native" -> "STM32"
      ~ CONFIG_OSAL_BACKEND: BAREMETAL -> FREERTOS

    Unchanged symbols: 42

    Summary:
      Total symbols in old.config: 47
      Total symbols in new.config: 48
      Added: 3
      Removed: 2
      Changed: 4
      Unchanged: 42

**JSON format:**

.. code-block:: json

    {
      "added": [
        {"symbol": "CONFIG_NEW_FEATURE", "value": "y"},
        {"symbol": "CONFIG_NEW_BUFFER_SIZE", "value": "256"}
      ],
      "removed": [
        {"symbol": "CONFIG_OLD_FEATURE", "value": "y"}
      ],
      "changed": [
        {
          "symbol": "CONFIG_UART1_BAUDRATE",
          "old_value": "9600",
          "new_value": "115200"
        }
      ],
      "unchanged": 42,
      "summary": {
        "total_old": 47,
        "total_new": 48,
        "added": 2,
        "removed": 1,
        "changed": 1
      }
    }

**HTML format:**

Generates an HTML report with color-coded differences:

* Green: Added symbols
* Red: Removed symbols
* Yellow: Changed symbols
* Gray: Unchanged symbols

Use Cases
^^^^^^^^^

**Compare platform configurations:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py \
        platforms/native/defconfig \
        platforms/STM32/defconfig_stm32f4

**Compare before/after changes:**

.. code-block:: bash

    cp .config .config.backup
    # Make changes
    python scripts/Kconfig/kconfig_diff.py .config.backup .config

**Review configuration updates:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py \
        old_version/.config \
        new_version/.config

Integration with Version Control
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Git diff tool:**

.. code-block:: bash

    # .gitconfig
    [diff "Kconfig"]
        command = python scripts/Kconfig/kconfig_diff.py

**Pre-merge check:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py \
        main:.config \
        feature-branch:.config


generate_config_docs.py
-----------------------

Overview
^^^^^^^^

Generates documentation from Kconfig files, creating a reference guide for all configuration options.

Usage
^^^^^

**Basic usage:**

.. code-block:: bash

    python scripts/Kconfig/generate_config_docs.py Kconfig \
        --output docs/config_reference.md

**With options:**

.. code-block:: bash

    python scripts/Kconfig/generate_config_docs.py Kconfig \
        --output docs/config_reference.rst \
        --format rst \
        --include-defaults

Command-Line Options
^^^^^^^^^^^^^^^^^^^^

.. code-block:: text

    KCONFIG_FILE           Root Kconfig file
    --output PATH          Output documentation file
    --format FORMAT        Output format (markdown, rst, html)
    --include-defaults     Include default values
    --include-help         Include help text
    --group-by GROUP       Group by (platform, module, type)
    --verbose              Enable verbose output
    --help                 Show help message

Examples
^^^^^^^^

**Markdown format:**

.. code-block:: bash

    python scripts/Kconfig/generate_config_docs.py Kconfig \
        --output docs/config_reference.md \
        --format markdown

**reStructuredText format:**

.. code-block:: bash

    python scripts/Kconfig/generate_config_docs.py Kconfig \
        --output docs/config_reference.rst \
        --format rst

**HTML format:**

.. code-block:: bash

    python scripts/Kconfig/generate_config_docs.py Kconfig \
        --output docs/config_reference.html \
        --format html

**Group by platform:**

.. code-block:: bash

    python scripts/Kconfig/generate_config_docs.py Kconfig \
        --output docs/config_reference.md \
        --group-by platform

Output Format
^^^^^^^^^^^^^

**Markdown example:**

.. code-block:: markdown

    # Configuration Reference

    ## Platform Configuration

    ### CONFIG_PLATFORM_STM32

    **Type:** bool
    **Default:** n
    **Depends on:** None

    Enable STM32 platform support.

    The STM32 platform provides support for STMicroelectronics
    STM32 series microcontrollers.

    ### CONFIG_STM32F407

    **Type:** bool
    **Default:** n
    **Depends on:** CONFIG_PLATFORM_STM32, CONFIG_STM32F4

    Enable STM32F407 variant.

    ## UART Configuration

    ### CONFIG_UART1_BAUDRATE

    **Type:** int
    **Default:** 115200
    **Range:** 9600 to 921600
    **Depends on:** CONFIG_INSTANCE_STM32_UART_1

    Baud rate for UART1 in bits per second.

**reStructuredText example:**

.. code-block:: rst

    Configuration Reference
    =======================

    Platform Configuration
    ----------------------

    CONFIG_PLATFORM_STM32
    ^^^^^^^^^^^^^^^^^^^^^

    :Type: bool
    :Default: n
    :Depends on: None

    Enable STM32 platform support.

    The STM32 platform provides support for STMicroelectronics
    STM32 series microcontrollers.

Grouping Options
^^^^^^^^^^^^^^^^

**By platform:**

* Native Platform
* STM32 Platform
* GD32 Platform
* ESP32 Platform

**By module:**

* HAL Configuration
* OSAL Configuration
* Peripheral Configuration
* Framework Configuration

**By type:**

* Boolean Options
* Integer Options
* String Options
* Hexadecimal Options

Integration with Sphinx
^^^^^^^^^^^^^^^^^^^^^^^

**Include in Sphinx documentation:**

.. code-block:: rst

    .. include:: config_reference.rst

**Automatic generation:**

.. code-block:: python

    # conf.py
    import subprocess

    subprocess.run([
        'python', 'scripts/Kconfig/generate_config_docs.py',
        'Kconfig',
        '--output', 'docs/sphinx/reference/config_reference.rst',
        '--format', 'rst'
    ])

Tool Workflow
-------------

Typical Workflow
^^^^^^^^^^^^^^^^

**1. Validate Kconfig files:**

.. code-block:: bash

    python scripts/Kconfig/validate_kconfig.py Kconfig

**2. Generate configuration:**

.. code-block:: bash

    python scripts/Kconfig/generate_config.py --config .config

**3. Compare configurations:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_diff.py old.config new.config

**4. Migrate if needed:**

.. code-block:: bash

    python scripts/Kconfig/kconfig_migrate.py \
        --input old.config \
        --output new.config \
        --version 2.0

**5. Generate documentation:**

.. code-block:: bash

    python scripts/Kconfig/generate_config_docs.py Kconfig \
        --output docs/config_reference.md

CI/CD Integration
^^^^^^^^^^^^^^^^^

**Complete CI pipeline:**

.. code-block:: yaml

    name: Kconfig CI

    on: [push, pull_request]

    jobs:
      validate:
        runs-on: ubuntu-latest
        steps:
          - uses: actions/checkout@v3

          - name: Validate Kconfig
            run: |
              python scripts/Kconfig/validate_kconfig.py Kconfig

          - name: Generate configuration
            run: |
              python scripts/Kconfig/generate_config.py

          - name: Generate documentation
            run: |
              python scripts/Kconfig/generate_config_docs.py Kconfig \
                --output docs/config_reference.md

          - name: Upload documentation
            uses: actions/upload-artifact@v3
            with:
              name: config-docs
              path: docs/config_reference.md

Best Practices
--------------

General Guidelines
^^^^^^^^^^^^^^^^^^

* Validate before generating
* Use version control for configurations
* Document configuration changes
* Test configurations before deployment
* Use default configurations as templates
* Keep configurations organized

Configuration Management
^^^^^^^^^^^^^^^^^^^^^^^^

* Use defconfig files for platforms
* Version control .config files
* Document custom configurations
* Use meaningful symbol names
* Group related options
* Provide comprehensive help text

Tool Usage
^^^^^^^^^^

* Run validation regularly
* Use diff to review changes
* Migrate configurations systematically
* Generate documentation automatically
* Integrate with CI/CD
* Monitor for errors

Troubleshooting
---------------

Common Issues
^^^^^^^^^^^^^

**Issue: Tool not found**

.. code-block:: bash

    # Ensure Python is installed
    python --version

    # Ensure tools are in correct location
    ls scripts/Kconfig/

**Issue: Permission denied**

.. code-block:: bash

    # Make scripts executable
    chmod +x scripts/Kconfig/*.py

**Issue: Module not found**

.. code-block:: bash

    # Install required packages
    pip install -r requirements.txt

**Issue: Invalid configuration**

.. code-block:: bash

    # Validate configuration
    python scripts/Kconfig/validate_kconfig.py Kconfig --check-values --config .config

    # Fix errors and regenerate
    python scripts/Kconfig/generate_config.py

See Also
--------

* :doc:`kconfig_tutorial` - Kconfig tutorial
* :doc:`kconfig_peripherals` - Peripheral configuration
* :doc:`kconfig_platforms` - Platform-specific configuration
* :doc:`../development/contributing` - Contributing guidelines
