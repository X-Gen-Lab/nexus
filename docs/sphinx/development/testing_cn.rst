测试
====

Nexus 使用 Google Test 进行单元测试。

运行测试
--------

.. code-block:: bash

    cmake -B build -DNEXUS_PLATFORM=native
    cmake --build build
    cd build && ctest --output-on-failure

编写测试
--------

.. code-block:: cpp

    #include <gtest/gtest.h>

    extern "C" {
    #include "hal/hal_gpio.h"
    }

    TEST(HalGpio, Init) {
        hal_gpio_config_t config = { ... };
        EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 0, &config));
    }

覆盖率
------

使用 ``-DNEXUS_ENABLE_COVERAGE=ON`` 启用覆盖率。
