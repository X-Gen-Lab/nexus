Native Platform Guide
=====================

Native executes host contract tests and external example applications.
``soc/native/controllers`` contains virtual peripheral implementations;
``soc/native/resources`` contains modeled resource managers;
``platforms/native`` contains lifecycle and object assembly. This organizational
SoC directory does not describe physical silicon.

Build and Test
--------------

.. code-block:: bash

   git clone --recurse-submodules https://github.com/X-Gen-Lab/nexus.git
   cd nexus
   python3 -m pip install kconfiglib==14.1.0
   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug --parallel 4
   ctest --preset linux-gcc-debug --output-on-failure --no-tests=error --parallel 4

The full profile explicitly selects OpenSSL and needs its development package.
``native-minimal-debug`` builds platform libraries without optional services or
OpenSSL. Retained Windows/macOS presets need execution in those environments;
Linux checks do not qualify them.

Contracts and Limitations
-------------------------

Typed GPIO/UART/SPI/I2C/Flash use explicit owner/generation and operation/child
leases. Native I2C is a behavior model, not an MCU hardware provider. Additional
legacy virtual peripherals are explicit host test/development models. OSAL Native
uses real host threads and dynamic resources; FreeRTOS POSIX tests execute the
pinned kernel but do not exercise the ARM interrupt port.

No simulated DMA/ISR, host elapsed time or faulting vendor replacement establishes
physical interrupt priority, electrical waveform, clock accuracy, power loss or
hard real-time behavior. Test helper headers are private development surfaces,
not public application headers. Real examples are maintained in the external
`nexus-examples repository <https://github.com/X-Gen-Lab/nexus-examples>`_.
