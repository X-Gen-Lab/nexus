Troubleshooting
===============

Common issues and solutions for Nexus development.

.. contents:: Table of Contents
   :local:
   :depth: 3

Build Issues
------------

CMake Configuration Fails
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: CMake configuration error

**Common Causes**:

1. Missing toolchain
2. Incorrect CMake version
3. Missing dependencies

**Solutions**:

.. code-block:: bash

   # Check CMake version (requires 3.20+)
   cmake --version

   # Install correct CMake version
   pip install cmake --upgrade

   # Verify toolchain is in PATH
   arm-none-eabi-gcc --version

   # Clean and reconfigure
   rm -rf build
   python scripts/building/build.py

Compilation Errors
~~~~~~~~~~~~~~~~~~

**Symptom**: Compilation fails with errors

**Common Causes**:

1. Missing header files
2. Syntax errors
3. Incompatible compiler flags

**Solutions**:

.. code-block:: bash

   # Check for syntax errors
   python scripts/tools/format.py --check

   # Verify include paths
   python scripts/building/build.py --verbose

   # Check compiler version
   arm-none-eabi-gcc --version

Linker Errors
~~~~~~~~~~~~~

**Symptom**: Undefined reference errors

**Common Causes**:

1. Missing source files
2. Incorrect library order
3. Missing symbols

**Solutions**:

.. code-block:: bash

   # Check if all sources are included
   grep -r "function_name" platforms/

   # Verify library dependencies in CMakeLists.txt
   # Check linker map file
   cat build/platform/application.map

Runtime Issues
--------------

Hard Fault
~~~~~~~~~~

**Symptom**: System crashes with hard fault

**Common Causes**:

1. Stack overflow
2. Null pointer dereference
3. Unaligned memory access
4. Invalid function pointer

**Debugging Steps**:

.. code-block:: bash

   # Enable fault handlers
   # Check fault status registers
   # Examine call stack in debugger
   arm-none-eabi-gdb build/application.elf
   (gdb) bt

**Solutions**:

* Increase stack size in linker script
* Add null pointer checks
* Use proper alignment
* Validate function pointers

Memory Issues
~~~~~~~~~~~~~

**Symptom**: Random crashes or corruption

**Common Causes**:

1. Buffer overflow
2. Memory leak
3. Use after free
4. Stack/heap collision

**Debugging**:

.. code-block:: bash

   # Run with AddressSanitizer (native)
   python scripts/building/build.py --sanitizer address
   ./build/native/debug/tests/test

   # Check memory usage
   arm-none-eabi-size build/application.elf

**Solutions**:

* Add bounds checking
* Track allocations/frees
* Use static analysis tools
* Increase heap size

Peripheral Issues
-----------------

GPIO Not Working
~~~~~~~~~~~~~~~~

**Checklist**:

☐ Clock enabled for GPIO port
☐ Pin configured correctly
☐ No conflicting alternate function
☐ Pull-up/down configured if needed
☐ Output driver enabled

**Debug Steps**:

.. code-block:: c

   /* Verify clock is enabled */
   if (!(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN)) {
       LOG_ERROR("GPIO clock not enabled");
   }

   /* Check pin configuration */
   LOG_DEBUG("MODER: 0x%08X", GPIOA->MODER);
   LOG_DEBUG("ODR: 0x%08X", GPIOA->ODR);

UART Not Transmitting
~~~~~~~~~~~~~~~~~~~~~

**Checklist**:

☐ UART clock enabled
☐ Baud rate configured correctly
☐ TX pin configured as alternate function
☐ UART enabled
☐ TX buffer not full

**Debug Steps**:

.. code-block:: c

   /* Check UART status */
   LOG_DEBUG("SR: 0x%08X", USART1->SR);
   LOG_DEBUG("CR1: 0x%08X", USART1->CR1);

   /* Verify baud rate */
   uint32_t brr = USART1->BRR;
   uint32_t baudrate = SystemCoreClock / brr;
   LOG_DEBUG("Calculated baudrate: %u", baudrate);

Testing Issues
--------------

Tests Failing
~~~~~~~~~~~~~

**Symptom**: Unit tests fail

**Common Causes**:

1. Test dependencies
2. Incorrect test setup
3. Platform-specific behavior
4. Timing issues

**Solutions**:

.. code-block:: bash

   # Run specific test
   python scripts/test/test.py --suite hal --test gpio

   # Run with verbose output
   python scripts/test/test.py --verbose

   # Check test logs
   cat build/test_results.log

Coverage Not Generated
~~~~~~~~~~~~~~~~~~~~~~

**Symptom**: Coverage report empty

**Solutions**:

.. code-block:: bash

   # Build with coverage enabled
   python scripts/building/build.py --coverage

   # Run tests
   python scripts/test/test.py

   # Generate report
   python scripts/test/coverage.py

   # View report
   open build/coverage/index.html

See Also
--------

* :doc:`debugging_guide` - Debugging techniques
* :doc:`testing` - Testing guidelines
* :doc:`build_system` - Build system documentation

Summary
-------

This guide covers common issues in:

* Build system (CMake, compilation, linking)
* Runtime (hard faults, memory issues)
* Peripherals (GPIO, UART, etc.)
* Testing (test failures, coverage)

For additional help, check the documentation or ask in GitHub Discussions.
