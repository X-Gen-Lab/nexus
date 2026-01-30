# Kconfig 快速入门指南

## 5 分钟快速开始

### 1. Native 平台开发（默认）

```bash
# 步骤 1: 配置（可选，使用默认配置）
# make menuconfig

# 步骤 2: 配置 CMake
cmake --preset windows-msvc-debug    # Windows
# 或
cmake --preset linux-gcc-debug       # Linux
# 或
cmake --preset macos-clang-debug     # macOS

# 步骤 3: 构建
cmake --build build

# 步骤 4: 运行测试
cd build && ctest
```

### 2. STM32 嵌入式开发

```bash
# 步骤 1: 配置 Kconfig
make menuconfig

# 在 menuconfig 中选择:
# Platform Configuration -> STM32 Platform
# Toolchain Configuration -> ARM GCC
# Toolchain Configuration -> ARM CPU Configuration
#   -> CPU Architecture: Cortex-M4
#   -> FPU Type: FPv4-SP
#   -> Float ABI: Hard

# 步骤 2: 配置 CMake（使用工具链文件）
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake

# 步骤 3: 构建
cmake --build build

# 步骤 4: 查看生成的文件
ls build/applications/*/
# 会生成 .elf, .bin, .hex 文件
```

## 常用配置场景

### 场景 1: 修改构建类型

```bash
# 方法 1: 使用 menuconfig
make menuconfig
# Build Configuration -> Build Type -> Release

# 方法 2: 使用 CMake 命令行
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

### 场景 2: 启用代码覆盖率

```bash
# 方法 1: 使用 menuconfig
make menuconfig
# Build Configuration -> Enable code coverage -> Yes

# 方法 2: 使用 CMake 命令行
cmake -B build -DNEXUS_ENABLE_COVERAGE=ON
```

### 场景 3: 切换平台

```bash
# 使用 menuconfig
make menuconfig
# Platform Configuration -> Target Platform -> STM32 Platform

# 重新配置 CMake
rm -rf build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake
```

### 场景 4: 配置链接脚本

```bash
# 使用 menuconfig
make menuconfig
# Toolchain Configuration -> Linker Configuration
#   -> Linker script path: platforms/stm32/linker/STM32F407VGTx_FLASH.ld
#   -> Stack size: 0x1000
#   -> Heap size: 0x800
```

## 配置文件管理

### 保存配置

```bash
# 保存当前配置为 defconfig
make savedefconfig

# 保存到指定位置
make savedefconfig DEFCONFIG=my_config
```

### 加载配置

```bash
# 加载 defconfig
make defconfig

# 加载指定配置
make defconfig DEFCONFIG=platforms/stm32/defconfig_stm32f4
```

### 查看配置

```bash
# 查看当前配置
cat .config

# 查看生成的 C 头文件
cat nexus_config.h
```

## 常见问题快速解决

### Q: 配置后 CMake 没有更新？

```bash
# 删除 CMake 缓存
rm -rf build/CMakeCache.txt

# 重新配置
cmake -B build
```

### Q: 工具链配置不生效？

```bash
# 删除整个构建目录
rm -rf build

# 重新配置（工具链必须在首次配置时指定）
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake
```

### Q: 如何查看所有配置选项？

```bash
# 使用 menuconfig
make menuconfig

# 或查看 Kconfig 文件
cat Kconfig
cat platforms/Kconfig
```

### Q: 如何覆盖 Kconfig 配置？

```bash
# 使用 CMake 命令行参数
cmake -B build -DNEXUS_PLATFORM=stm32 -DCMAKE_BUILD_TYPE=Release
```

## 配置检查清单

在构建前，检查以下配置是否正确：

- [ ] 平台选择正确（Native/STM32/GD32/ESP32/NRF52）
- [ ] 工具链与平台匹配
- [ ] 构建类型符合需求（Debug/Release）
- [ ] 嵌入式平台配置了链接脚本
- [ ] CPU/FPU 配置与目标芯片匹配
- [ ] 栈和堆大小合理

## 查看配置摘要

```bash
# 配置 CMake 并查看摘要
cmake -B build 2>&1 | tail -40

# 输出示例:
# ========================================
# === Nexus Build Configuration ===
# ========================================
#   Version:        0.1.0
#   Platform:       native
#   OSAL Backend:   baremetal
#   Build Type:     Debug
#   Compiler:       MSVC 19.40.33811.0
#   Generator:      Visual Studio
# ----------------------------------------
#   Tests:          ON
#   Examples:       ON
#   Documentation:  OFF
#   Coverage:       OFF
#   Build Cache:    OFF
# ----------------------------------------
#   Parallel Jobs:  7
#   Binary Dir:     D:/code/nexus/nexus/build
# ========================================
```

## 推荐工作流程

### 开发阶段

```bash
# 1. 使用 Debug 构建
make menuconfig  # 选择 Debug

# 2. 启用测试和覆盖率
# Build Configuration -> Build tests: Yes
# Build Configuration -> Enable coverage: Yes

# 3. 配置和构建
cmake --preset linux-gcc-debug
cmake --build build

# 4. 运行测试
cd build && ctest --output-on-failure
```

### 发布阶段

```bash
# 1. 使用 Release 构建
make menuconfig  # 选择 Release

# 2. 禁用测试和调试功能
# Build Configuration -> Build tests: No
# Build Configuration -> Enable coverage: No

# 3. 配置和构建
cmake --preset cross-arm-release
cmake --build build

# 4. 检查生成的二进制文件
ls -lh build/applications/*/*.bin
```

## 下一步

- 阅读 [Kconfig 配置系统设计](kconfig-design.md) 了解详细配置选项
- 阅读 [CMake 与 Kconfig 集成](cmake-kconfig-integration.md) 了解集成原理
- 查看 [平台配置文档](../platforms/) 了解平台特定配置

## 获取帮助

如果遇到问题：

1. 查看 [修复记录](kconfig-cmake-fixes.md)
2. 检查配置摘要输出
3. 查看 CMake 详细日志: `cmake -B build --trace-expand`
4. 提交 Issue 并附上配置文件和错误日志
