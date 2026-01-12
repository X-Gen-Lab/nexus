Testing
=======

Nexus uses Google Test for unit testing.

Running Tests
-------------

.. code-block:: bash

    cmake -B build -DNEXUS_PLATFORM=native
    cmake --build build
    cd build && ctest --output-on-failure

Writing Tests
-------------

.. code-block:: cpp

    #include <gtest/gtest.h>

    extern "C" {
    #include "hal/hal_gpio.h"
    }

    TEST(HalGpio, Init) {
        hal_gpio_config_t config = { ... };
        EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 0, &config));
    }

Coverage
--------

Enable coverage with ``-DNEXUS_ENABLE_COVERAGE=ON``.
