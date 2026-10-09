External Examples Tour
======================

Applications are maintained in the separate
`nexus-examples repository <https://github.com/X-Gen-Lab/nexus-examples>`_.
Its Gitlink and lock fix the SDK; actual source-pair reports identify tested
applications/Boards/configurations. Nexus platform contract tests do not import
that repository's business sources.

.. list-table:: Example responsibility
   :header-rows: 1
   :widths: 30 70

   * - Target
     - Demonstrates
   * - ``boot_blinky``
     - Typed Board GPIO, explicit bootstrap and finite host execution.
   * - ``uart_echo``
     - Events/tickets, bounded echo and buffer settlement where supported.
   * - ``rtos_pipeline``
     - Checked worker creation, bounded producer/consumer and synchronization.
   * - ``industrial_model``
     - Host Modbus/virtual plant and product health policy outside the platform.
   * - ``shell_console``
     - Explicit Shell adapter and smoke execution.
   * - ``config_roundtrip``
     - Types/namespaces and volatile RAM roundtrip, not Flash persistence.
   * - ``log_uart``
     - Caller-owned bounded UART logging, pump/flush and retained leases.
   * - ``spi_transaction``
     - Typed parent/child, sync/queued/cancel settlement and reviewed fixture.

Run in that repository:

.. code-block:: bash

   git submodule update --init --recursive
   python3 scripts/test_repository.py
   python3 scripts/build.py --preset native-debug --test

See its README/Board guide for which targets are created for each configuration.
Compiled targets are not physical qualification. Boards are currently deferred;
examples use no default Flash erase, product trust/signing or manufacturing keys.
