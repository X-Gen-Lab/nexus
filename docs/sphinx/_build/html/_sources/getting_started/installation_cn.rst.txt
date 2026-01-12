安装指南
========

前置条件
--------

安装 Nexus 之前，请确保已安装以下工具：

**必需：**

- CMake 3.16 或更高版本
- C 编译器 (GCC、Clang 或 MSVC)
- Git

**ARM 目标平台：**

- ARM GCC 工具链 (arm-none-eabi-gcc)
- OpenOCD 或 J-Link 用于调试

**文档生成：**

- Doxygen 1.9+
- Python 3.8+
- Sphinx 和 Breathe

安装前置条件
------------

**Windows:**

.. code-block:: powershell

    # 使用 Chocolatey 安装
    choco install cmake git
    choco install gcc-arm-embedded

**Linux (Ubuntu/Debian):**

.. code-block:: bash

    sudo apt update
    sudo apt install cmake git build-essential
    sudo apt install gcc-arm-none-eabi

**macOS:**

.. code-block:: bash

    brew install cmake git
    brew install --cask gcc-arm-embedded

获取 Nexus
----------

克隆仓库：

.. code-block:: bash

    git clone https://github.com/nexus-team/nexus.git
    cd nexus

构建
----

**本机构建（用于测试）：**

.. code-block:: bash

    cmake -B build -DNEXUS_PLATFORM=native
    cmake --build build

**STM32F4 交叉编译：**

.. code-block:: bash

    cmake -B build-stm32f4 \
        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
        -DNEXUS_PLATFORM=stm32f4
    cmake --build build-stm32f4

运行测试
--------

.. code-block:: bash

    cd build
    ctest --output-on-failure

构建文档
--------

.. code-block:: bash

    # 生成 API 文档
    doxygen Doxyfile

    # 构建 Sphinx 文档
    cd docs/sphinx
    sphinx-build -b html . _build/html

下一步
------

- :doc:`quickstart_cn` - 构建你的第一个 Nexus 应用
- :doc:`../user_guide/architecture_cn` - 了解平台架构
