Effective Configuration
=======================

Kconfig resolves one explicit configuration per build directory. It supplies
software/backend selection and bounded resource settings; CMake presets own
compiler/build mode and target usage requirements own include/link context.
There is no separate configuration DSL or source-root ``.config`` fallback.

.. code-block:: bash

   cmake --list-presets
   cmake --preset native-minimal-debug
   cmake --build --preset native-minimal-debug --parallel 4

``NEXUS_CONFIG_FILE`` names the fragment. One parse emits build-local
effective.config/header/CMake outputs. Unknown/duplicate symbols, unsupported
choices, illegal ranges/dependencies and contradictory values stop configuration.
Failed regeneration must not let stale outputs compile. Reconfiguration removes
obsolete CONFIG cache entries.

The maintained platform choices are Native/F407VE-VG-ZG/F470ZG; OSAL choices are
Native/baremetal/FreeRTOS with actual platform constraints. Controller choices
select implemented providers, not every peripheral in vendor SDK. Board manifest
checks reviewed GPIO/UART/SPI resource bindings and density/HSE. External layout
owns image/regions; whole-Flash image/no regions is the default, offset zero only.

See :doc:`kconfig_platforms`, :doc:`kconfig_peripherals`, :doc:`kconfig_osal` and
:doc:`kconfig_tutorial`. Public source/config changes need negative configuration
checks and actual affected images/consumers, not a successful generated draft.
