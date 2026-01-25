Kconfig Tools API
=================

Overview
--------

The Kconfig tools provide Python APIs for managing configuration files,
validating Kconfig syntax, and generating C header files.

Module: generate_hal_config
---------------------------

Configuration Header Generation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. py:function:: generate_header(config_dict, output_file=None)

   Generate C header file from configuration dictionary.

   :param dict config_dict: Configuration key-value pairs
   :param str output_file: Output file path (optional)
   :return: Generated header content as string
   :rtype: str

   **Example:**

   .. code-block:: python

       from generate_hal_config import generate_header

       config = {
           'CONFIG_PLATFORM_STM32': 'y',
           'CONFIG_UART1_BAUDRATE': '115200',
           'CONFIG_OSAL_FREERTOS': 'y'
       }

       header = generate_header(config, 'nexus_config.h')

.. py:function:: parse_config_file(config_path)

   Parse .config file and return configuration dictionary.

   :param str config_path: Path to .config file
   :return: Configuration dictionary
   :rtype: dict

   **Example:**

   .. code-block:: python

       from generate_hal_config import parse_config_file

       config = parse_config_file('.config')
       print(f"Platform: {config.get('CONFIG_PLATFORM_NAME')}")

.. py:function:: generate_default_config(platform=None)

   Generate default configuration for specified platform.

   :param str platform: Platform name (native, STM32, GD32, etc.)
   :return: Default configuration dictionary
   :rtype: dict

   **Example:**

   .. code-block:: python

       from generate_hal_config import generate_default_config

       # Generate default for STM32
       config = generate_default_config('STM32')

       # Generate default for current platform
       config = generate_default_config()

Module: validate_kconfig
------------------------

Kconfig Validation
^^^^^^^^^^^^^^^^^^

.. py:class:: KconfigValidator(root_dir='.')

   Validator for Kconfig files.

   :param str root_dir: Root directory for resolving paths

   .. py:method:: validate_file(kconfig_path, is_included=False)

      Validate a single Kconfig file.

      :param Path kconfig_path: Path to Kconfig file
      :param bool is_included: Whether file is included via source/rsource
      :return: Tuple of (errors, warnings)
      :rtype: tuple[list[str], list[str]]

      **Example:**

      .. code-block:: python

          from pathlib import Path
          from validate_kconfig import KconfigValidator

          validator = KconfigValidator('.')
          errors, warnings = validator.validate_file(Path('Kconfig'))

          if errors:
              print(f"Errors: {errors}")
          if warnings:
              print(f"Warnings: {warnings}")

   .. py:method:: validate_all(kconfig_file)

      Validate Kconfig file and all included files.

      :param Path kconfig_file: Root Kconfig file path
      :return: True if validation passed, False otherwise
      :rtype: bool

      **Example:**

      .. code-block:: python

          from pathlib import Path
          from validate_kconfig import KconfigValidator

          validator = KconfigValidator('.')
          success = validator.validate_all(Path('Kconfig'))

          if not success:
              validator.print_results()

   .. py:method:: validate_dependencies()

      Validate dependencies and selects.

      :return: Tuple of (errors, warnings)
      :rtype: tuple[list[str], list[str]]

   .. py:method:: validate_ranges()

      Validate range constraints and default values.

      :return: Tuple of (errors, warnings)
      :rtype: tuple[list[str], list[str]]

   .. py:method:: print_results()

      Print validation results to stdout.

   .. py:attribute:: errors

      List of validation errors.

      :type: list[str]

   .. py:attribute:: warnings

      List of validation warnings.

      :type: list[str]

   .. py:attribute:: symbols

      Dictionary of parsed configuration symbols.

      :type: dict[str, dict]

   .. py:attribute:: dependencies

      Dictionary of symbol dependencies.

      :type: dict[str, set[str]]

Module: kconfig_migrate
-----------------------

Configuration Migration
^^^^^^^^^^^^^^^^^^^^^^^

.. py:function:: migrate_config(input_file, output_file, target_version=None)

   Migrate configuration file to new version.

   :param str input_file: Input configuration file path
   :param str output_file: Output configuration file path
   :param str target_version: Target version (optional)
   :return: True if migration succeeded
   :rtype: bool

   **Example:**

   .. code-block:: python

       from kconfig_migrate import migrate_config

       success = migrate_config(
           'old.config',
           'new.config',
           target_version='2.0'
       )

       if success:
           print("Migration completed successfully")

.. py:function:: detect_version(config_file)

   Detect configuration file version.

   :param str config_file: Configuration file path
   :return: Version string or None
   :rtype: str or None

.. py:function:: get_symbol_mapping(from_version, to_version)

   Get symbol mapping between versions.

   :param str from_version: Source version
   :param str to_version: Target version
   :return: Symbol mapping dictionary
   :rtype: dict[str, str]

Module: kconfig_diff
--------------------

Configuration Comparison
^^^^^^^^^^^^^^^^^^^^^^^^

.. py:function:: diff_configs(config1_path, config2_path, format='text')

   Compare two configuration files.

   :param str config1_path: First configuration file path
   :param str config2_path: Second configuration file path
   :param str format: Output format ('text' or 'json')
   :return: Difference report
   :rtype: str or dict

   **Example:**

   .. code-block:: python

       from kconfig_diff import diff_configs

       # Text format
       diff_text = diff_configs('.config', 'platforms/native/.config')
       print(diff_text)

       # JSON format
       diff_json = diff_configs(
           '.config',
           'platforms/native/.config',
           format='json'
       )

.. py:function:: parse_config(config_path)

   Parse configuration file.

   :param str config_path: Configuration file path
   :return: Configuration dictionary
   :rtype: dict

.. py:function:: format_diff_text(diff_dict)

   Format difference dictionary as text.

   :param dict diff_dict: Difference dictionary
   :return: Formatted text
   :rtype: str

.. py:function:: format_diff_json(diff_dict)

   Format difference dictionary as JSON.

   :param dict diff_dict: Difference dictionary
   :return: JSON string
   :rtype: str

Module: generate_config_docs
----------------------------

Documentation Generation
^^^^^^^^^^^^^^^^^^^^^^^^

.. py:function:: generate_docs(kconfig_file, output_file=None, format='markdown')

   Generate documentation from Kconfig files.

   :param str kconfig_file: Root Kconfig file path
   :param str output_file: Output file path (optional)
   :param str format: Output format ('markdown' or 'rst')
   :return: Generated documentation
   :rtype: str

   **Example:**

   .. code-block:: python

       from generate_config_docs import generate_docs

       # Generate markdown documentation
       docs = generate_docs('Kconfig', 'config_reference.md')

       # Generate reStructuredText
       docs = generate_docs('Kconfig', 'config_reference.rst', format='rst')

.. py:function:: extract_config_options(kconfig_file)

   Extract all configuration options from Kconfig file.

   :param str kconfig_file: Kconfig file path
   :return: List of configuration options
   :rtype: list[dict]

.. py:function:: format_option_markdown(option)

   Format configuration option as markdown.

   :param dict option: Configuration option dictionary
   :return: Formatted markdown
   :rtype: str

.. py:function:: format_option_rst(option)

   Format configuration option as reStructuredText.

   :param dict option: Configuration option dictionary
   :return: Formatted reStructuredText
   :rtype: str

Command-Line Interface
----------------------

nexus_config.py
^^^^^^^^^^^^^^^

Unified configuration management tool.

**Usage:**

.. code-block:: bash

    python scripts/nexus_config.py <command> [options]

**Commands:**

* ``generate`` - Generate configuration header file
* ``validate`` - Validate Kconfig files
* ``migrate`` - Migrate configuration to new version
* ``diff`` - Compare configuration files
* ``info`` - Display configuration information

**Generate Command:**

.. code-block:: bash

    python scripts/nexus_config.py generate [options]

    Options:
      -c, --config FILE    Input configuration file
      -o, --output FILE    Output header file
      -d, --default        Generate default configuration
      -p, --platform NAME  Platform name for default config

**Validate Command:**

.. code-block:: bash

    python scripts/nexus_config.py validate [kconfig_file]

    Arguments:
      kconfig_file         Root Kconfig file (default: Kconfig)

**Migrate Command:**

.. code-block:: bash

    python scripts/nexus_config.py migrate input_file [options]

    Arguments:
      input_file           Input configuration file

    Options:
      -o, --output FILE    Output configuration file
      -t, --target-version VERSION  Target version

**Diff Command:**

.. code-block:: bash

    python scripts/nexus_config.py diff config1 config2 [options]

    Arguments:
      config1              First configuration file
      config2              Second configuration file

    Options:
      -f, --format FORMAT  Output format (text or json)
      -o, --output FILE    Output file

Data Structures
---------------

Configuration Dictionary
^^^^^^^^^^^^^^^^^^^^^^^^

Configuration dictionaries map symbol names to values:

.. code-block:: python

    {
        'CONFIG_PLATFORM_STM32': 'y',
        'CONFIG_STM32F407': 'y',
        'CONFIG_UART1_BAUDRATE': '115200',
        'CONFIG_UART1_MODE_DMA': 'y',
        'CONFIG_OSAL_FREERTOS': 'y',
        'CONFIG_OSAL_TICK_RATE_HZ': '1000'
    }

Symbol Information
^^^^^^^^^^^^^^^^^^

Symbol information dictionaries contain metadata:

.. code-block:: python

    {
        'name': 'UART1_BAUDRATE',
        'type': 'int',
        'default': '115200',
        'range': (9600, 921600),
        'help': 'Baud rate for UART1',
        'file': 'platforms/STM32/src/uart/Kconfig',
        'line': 42
    }

Validation Results
^^^^^^^^^^^^^^^^^^

Validation results contain errors and warnings:

.. code-block:: python

    {
        'errors': [
            'Kconfig:10: Unclosed if block',
            'platforms/STM32/Kconfig:25: Undefined symbol INVALID_SYMBOL'
        ],
        'warnings': [
            'platforms/native/Kconfig:15: default outside of config block'
        ]
    }

Difference Report
^^^^^^^^^^^^^^^^^

Difference reports show configuration changes:

.. code-block:: python

    {
        'added': {
            'CONFIG_NEW_FEATURE': 'y'
        },
        'removed': {
            'CONFIG_OLD_FEATURE': 'y'
        },
        'changed': {
            'CONFIG_UART1_BAUDRATE': {
                'old': '9600',
                'new': '115200'
            }
        },
        'unchanged': {
            'CONFIG_PLATFORM_STM32': 'y'
        }
    }

Error Handling
--------------

Exceptions
^^^^^^^^^^

.. py:exception:: KconfigError

   Base exception for Kconfig-related errors.

.. py:exception:: ValidationError

   Raised when validation fails.

.. py:exception:: ParseError

   Raised when parsing fails.

.. py:exception:: GenerationError

   Raised when header generation fails.

**Example:**

.. code-block:: python

    from validate_kconfig import KconfigValidator, ValidationError

    try:
        validator = KconfigValidator('.')
        success = validator.validate_all(Path('Kconfig'))
        if not success:
            raise ValidationError("Validation failed")
    except ValidationError as e:
        print(f"Error: {e}")
        validator.print_results()

Best Practices
--------------

Validation
^^^^^^^^^^

Always validate before generating:

.. code-block:: python

    from pathlib import Path
    from validate_kconfig import KconfigValidator
    from generate_hal_config import generate_header, parse_config_file

    # Validate first
    validator = KconfigValidator('.')
    if not validator.validate_all(Path('Kconfig')):
        print("Validation failed!")
        validator.print_results()
        exit(1)

    # Then generate
    config = parse_config_file('.config')
    generate_header(config, 'nexus_config.h')

Error Handling
^^^^^^^^^^^^^^

Handle errors gracefully:

.. code-block:: python

    try:
        config = parse_config_file('.config')
    except FileNotFoundError:
        print("Config file not found, using defaults")
        config = generate_default_config()
    except Exception as e:
        print(f"Error parsing config: {e}")
        exit(1)

Logging
^^^^^^^

Use logging for debugging:

.. code-block:: python

    import logging

    logging.basicConfig(level=logging.DEBUG)
    logger = logging.getLogger(__name__)

    validator = KconfigValidator('.')
    logger.debug("Starting validation")
    success = validator.validate_all(Path('Kconfig'))
    logger.info(f"Validation {'passed' if success else 'failed'}")

See Also
--------

* :doc:`../user_guide/kconfig` - Kconfig user guide
* :doc:`../development/contributing` - Contributing guidelines
* Scripts README: ``scripts/Kconfig/README.md``

