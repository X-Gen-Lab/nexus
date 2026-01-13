测试
====

Nexus 使用 Google Test 进行单元测试，为嵌入式软件开发提供完整的测试基础设施。

测试框架
--------

- **单元测试**: Google Test (gtest)
- **Mock**: Google Mock (gmock)
- **覆盖率**: gcov/lcov
- **静态分析**: MISRA C 检查器

构建测试
--------

配置并构建测试::

    cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
    cmake --build build --config Release

运行测试
--------

运行所有测试
~~~~~~~~~~~~

::

    ctest --test-dir build -C Release --output-on-failure

运行特定测试套件
~~~~~~~~~~~~~~~~

::

    ./build/tests/Release/nexus_tests --gtest_filter="HalGpioTest.*"

详细输出运行
~~~~~~~~~~~~

::

    ./build/tests/Release/nexus_tests --gtest_filter="*" --gtest_print_time=1

列出可用测试
~~~~~~~~~~~~

::

    ./build/tests/Release/nexus_tests --gtest_list_tests

编写测试
--------

测试文件位置
~~~~~~~~~~~~

测试文件放在 ``tests/`` 目录，结构如下::

    tests/
    ├── hal/
    │   ├── test_hal_gpio.cpp
    │   ├── test_hal_uart.cpp
    │   └── test_hal_spi.cpp
    ├── osal/
    │   ├── test_osal_task.cpp
    │   ├── test_osal_mutex.cpp
    │   └── test_osal_queue.cpp
    └── framework/
        └── log/
            └── test_log.cpp

测试文件模板
~~~~~~~~~~~~

.. code-block:: cpp

    /**
     * \file            test_hal_gpio.cpp
     * \brief           HAL GPIO 单元测试
     */

    #include <gtest/gtest.h>

    extern "C" {
    #include "hal/hal_gpio.h"
    }

    class HalGpioTest : public ::testing::Test {
    protected:
        void SetUp() override {
            /* 初始化测试夹具 */
        }

        void TearDown() override {
            /* 清理测试夹具 */
        }
    };

    TEST_F(HalGpioTest, InitOutput) {
        hal_gpio_config_t config = {
            .direction   = HAL_GPIO_DIR_OUTPUT,
            .pull        = HAL_GPIO_PULL_NONE,
            .output_mode = HAL_GPIO_OUTPUT_PP,
            .speed       = HAL_GPIO_SPEED_LOW,
            .init_level  = HAL_GPIO_LEVEL_LOW
        };
        EXPECT_EQ(HAL_OK, hal_gpio_init(HAL_GPIO_PORT_A, 0, &config));
    }

    TEST_F(HalGpioTest, InitNullConfig) {
        EXPECT_EQ(HAL_ERROR_NULL_POINTER,
                  hal_gpio_init(HAL_GPIO_PORT_A, 0, nullptr));
    }

    TEST_F(HalGpioTest, WriteOutput) {
        /* 设置 */
        hal_gpio_config_t config = {
            .direction = HAL_GPIO_DIR_OUTPUT
        };
        hal_gpio_init(HAL_GPIO_PORT_A, 0, &config);

        /* 测试 */
        EXPECT_EQ(HAL_OK, hal_gpio_write(HAL_GPIO_PORT_A, 0, HAL_GPIO_LEVEL_HIGH));
    }

测试命名规范
~~~~~~~~~~~~

- 测试类：``ModuleNameTest``（如 ``HalGpioTest``、``OsalMutexTest``）
- 测试用例：描述性动作（如 ``InitOutput``、``WriteInvalidPort``）

测试类别
~~~~~~~~

1. **正向测试**: 验证有效输入的正确行为
2. **负向测试**: 验证无效输入的错误处理
3. **边界测试**: 测试边界条件和极限值
4. **集成测试**: 测试组件间交互

代码覆盖率
----------

启用覆盖率
~~~~~~~~~~

::

    cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON -DNEXUS_ENABLE_COVERAGE=ON
    cmake --build build

生成覆盖率报告
~~~~~~~~~~~~~~

::

    cd build
    ctest --output-on-failure
    lcov --capture --directory . --output-file coverage.info
    lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
    genhtml coverage.info --output-directory coverage_report

覆盖率要求
~~~~~~~~~~

- 最低代码覆盖率：**90%**
- 所有公共 API 必须有测试
- 所有错误路径必须被测试

静态分析
--------

MISRA C 合规性
~~~~~~~~~~~~~~

运行 MISRA C 检查器::

    # 使用 cppcheck 的 MISRA 插件
    cppcheck --addon=misra hal/ osal/ framework/

地址消毒器
~~~~~~~~~~

使用 sanitizers 构建以检测内存错误::

    cmake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON \
          -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
          -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"

最佳实践
--------

1. **测试独立性**: 每个测试应独立，不依赖其他测试
2. **清晰断言**: 使用描述性断言消息
3. **Setup/Teardown**: 使用夹具进行通用设置和清理
4. **Mock 外部依赖**: 对硬件和 OS 依赖使用 mock
5. **测试边界情况**: 包含边界条件和错误情况
6. **保持测试快速**: 单元测试应快速运行
7. **避免测试重复**: 对相似情况使用参数化测试

参数化测试
~~~~~~~~~~

.. code-block:: cpp

    class HalGpioPortTest : public ::testing::TestWithParam<hal_gpio_port_t> {};

    TEST_P(HalGpioPortTest, InitValidPort) {
        hal_gpio_port_t port = GetParam();
        hal_gpio_config_t config = { .direction = HAL_GPIO_DIR_OUTPUT };
        EXPECT_EQ(HAL_OK, hal_gpio_init(port, 0, &config));
    }

    INSTANTIATE_TEST_SUITE_P(
        AllPorts,
        HalGpioPortTest,
        ::testing::Values(
            HAL_GPIO_PORT_A,
            HAL_GPIO_PORT_B,
            HAL_GPIO_PORT_C
        )
    );

CI 集成
-------

每个 PR 都会通过 GitHub Actions 自动运行测试：

- 多平台测试（Windows、Linux、macOS）
- 覆盖率报告
- 内存消毒器检查
- MISRA 合规性检查
