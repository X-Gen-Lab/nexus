# CMake 与 Kconfig 集成文档

## 概述

本文档描述 Nexus 项目中 CMake 构建系统与 Kconfig 配置系统的集成方式。

## 集成架构

```
┌─────────────────────────────────────────────────────────────┐
│                     用户配置界面                              │
│                  (menuconfig/guiconfig)                      │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                   Kconfig 文件                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Kconfig      │  │ platforms/   │  │ hal/         │      │
│  │ (根配置)     │  │ Kconfig      │  │ Kconfig      │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                   .config 文件                               │
│              (生成的配置文件)                                 │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              LoadKconfig.cmake                               │
│         (解析 .config 并设置 CMake 变量)                      │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              CMake 构建系统                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ CMakeLists   │  │ NexusPlatform│  │ Toolchain    │      │
│  │ .txt         │  │ .cmake       │  │ Files        │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

## 配置流程

### 1. Kconfig 配置阶段

用户通过 menuconfig 或 guiconfig 配置项目：

```bash
# 使用 menuconfig (文本界面)
make menuconfig

# 使用 guiconfig (图形界面)
make guiconfig
```

配置选项包括：
- 构建类型 (Debug/Release/MinSizeRel/RelWithDebInfo)
- 工具链选择 (GCC/Clang/MSVC/ARM-GCC/ARM-Clang/IAR)
- 平台选择 (Native/STM32/GD32/ESP32/NRF52)
- CPU/FPU 配置 (仅嵌入式平台)
- 链接脚本和内存配置

配置完成后生成 `.config` 文件。

### 2. CMake 配置阶段

CMake 在配置阶段读取 `.config` 文件：

```bash
# 配置项目
cmake -B build

# 或使用预设
cmake --preset linux-gcc-debug
```

CMake 执行以下步骤：

1. **加载 Kconfig 配置** (`LoadKconfig.cmake`)
   - 解析 `.config` 文件
   - 设置 `CONFIG_*` 变量

2. **应用配置到 CMake** (`apply_kconfig_to_cmake()`)
   - 映射 Kconfig 选项到 CMake 变量
   - 设置构建类型、平台、工具链等

3. **验证配置** (`nexus_validate_kconfig()`)
   - 检查配置一致性
   - 验证平台和工具链兼容性

4. **平台配置** (`nexus_configure_platform()`)
   - 检测主机和目标平台
   - 配置编译器和工具链
   - 设置平台特定标志

5. **应用编译器标志** (`apply_kconfig_compiler_flags()`)
   - 根据配置启用 sanitizers
   - 配置覆盖率选项

### 3. CMake 构建阶段

```bash
# 构建项目
cmake --build build

# 或使用 make/ninja
cd build && make -j$(nproc)
```

## Kconfig 到 CMake 变量映射

### 构建配置

| Kconfig 选项 | CMake 变量 | 说明 |
|-------------|-----------|------|
| CONFIG_BUILD_TYPE | CMAKE_BUILD_TYPE | 构建类型 |
| CONFIG_BUILD_TESTS | NEXUS_BUILD_TESTS | 是否构建测试 |
| CONFIG_BUILD_EXAMPLES | NEXUS_BUILD_EXAMPLES | 是否构建示例 |
| CONFIG_ENABLE_COVERAGE | NEXUS_ENABLE_COVERAGE | 是否启用覆盖率 |
| CONFIG_ENABLE_SANITIZERS | NEXUS_ENABLE_SANITIZERS | 是否启用 sanitizers |

### 平台配置

| Kconfig 选项 | CMake 变量 | 说明 |
|-------------|-----------|------|
| CONFIG_PLATFORM_NAME | NEXUS_PLATFORM | 平台名称 |
| CONFIG_PLATFORM_NATIVE | NEXUS_PLATFORM_NATIVE | Native 平台标志 |
| CONFIG_PLATFORM_STM32 | NEXUS_PLATFORM_STM32 | STM32 平台标志 |
| CONFIG_STM32_CHIP_NAME | NEXUS_STM32_CHIP | STM32 芯片型号 |

### 工具链配置

| Kconfig 选项 | CMake 变量 | 说明 |
|-------------|-----------|------|
| CONFIG_TOOLCHAIN_NAME | NEXUS_TOOLCHAIN_NAME | 工具链名称 |
| CONFIG_TOOLCHAIN_FILE | NEXUS_TOOLCHAIN_FILE | 工具链文件路径 |
| CONFIG_CPU_ARCH | NEXUS_CPU_ARCH | CPU 架构 |
| CONFIG_FPU_TYPE | NEXUS_FPU_TYPE | FPU 类型 |
| CONFIG_FLOAT_ABI | NEXUS_FLOAT_ABI | Float ABI |

### 链接器配置

| Kconfig 选项 | CMake 变量 | 说明 |
|-------------|-----------|------|
| CONFIG_LINKER_SCRIPT | NEXUS_LINKER_SCRIPT | 链接脚本路径 |
| CONFIG_STACK_SIZE | NEXUS_STACK_SIZE | 栈大小 |
| CONFIG_HEAP_SIZE | NEXUS_HEAP_SIZE | 堆大小 |

## 配置优先级

CMake 变量的设置遵循以下优先级（从高到低）：

1. **命令行参数**: `cmake -DNEXUS_PLATFORM=stm32`
2. **Kconfig 配置**: `.config` 文件中的设置
3. **CMake 缓存**: `CMakeCache.txt` 中的缓存值
4. **默认值**: CMakeLists.txt 中定义的默认值

## 配置验证

CMake 在配置阶段会验证以下内容：

### 1. 平台和工具链兼容性

```cmake
# 检查 ARM 工具链是否用于 Native 平台
if(ARM_TOOLCHAIN AND NATIVE_PLATFORM)
    message(FATAL_ERROR "ARM toolchain cannot be used with native platform")
endif()
```

### 2. CPU 和 FPU 兼容性

```cmake
# 检查 Cortex-M0/M3 是否配置了 FPU
if(CORTEX_M0_OR_M3 AND FPU_ENABLED)
    message(FATAL_ERROR "Cortex-M0/M3 do not support FPU")
endif()
```

### 3. 嵌入式平台链接脚本

```cmake
# 检查嵌入式平台是否配置了链接脚本
if(EMBEDDED_PLATFORM AND NOT LINKER_SCRIPT)
    message(WARNING "No linker script configured for embedded platform")
endif()
```

### 4. 平台特定功能

```cmake
# 检查测试是否在非 Native 平台启用
if(BUILD_TESTS AND NOT NATIVE_PLATFORM)
    message(WARNING "Tests are only supported on native platform")
endif()
```

## 使用示例

### 示例 1: Native 平台开发

```bash
# 1. 配置 Kconfig
make menuconfig
# 选择:
#   - Build Type: Debug
#   - Toolchain: GCC
#   - Platform: Native
#   - Build Tests: Yes
#   - Enable Coverage: Yes

# 2. 配置 CMake
cmake -B build

# 3. 构建
cmake --build build

# 4. 运行测试
cd build && ctest
```

### 示例 2: STM32F4 嵌入式开发

```bash
# 1. 配置 Kconfig
make menuconfig
# 选择:
#   - Build Type: Release
#   - Toolchain: ARM GCC
#   - Platform: STM32
#   - STM32 Chip: STM32F407
#   - CPU Arch: Cortex-M4
#   - FPU Type: FPv4-SP
#   - Float ABI: Hard
#   - Linker Script: platforms/stm32/linker/STM32F407VGTx_FLASH.ld

# 2. 配置 CMake (使用工具链文件)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake

# 3. 构建
cmake --build build

# 4. 生成二进制文件
# 自动生成 .bin 和 .hex 文件
```

### 示例 3: 使用 CMake 预设

```bash
# 1. 配置 Kconfig (可选，预设可能覆盖某些选项)
make menuconfig

# 2. 使用预设配置和构建
cmake --preset cross-arm-release
cmake --build --preset cross-arm-release
```

## 配置文件位置

```
nexus/
├── Kconfig                    # 根 Kconfig 文件
├── .config                    # 生成的配置文件
├── nexus_config.h            # 生成的 C 头文件
├── CMakeLists.txt            # 根 CMake 文件
├── CMakePresets.json         # CMake 预设
├── cmake/
│   ├── modules/
│   │   ├── LoadKconfig.cmake      # Kconfig 加载模块
│   │   ├── NexusKconfig.cmake     # Kconfig 集成模块
│   │   └── NexusPlatform.cmake    # 平台配置模块
│   └── toolchains/
│       ├── arm-none-eabi.cmake    # ARM GCC 工具链
│       ├── armclang.cmake         # ARM Clang 工具链
│       └── iar-arm.cmake          # IAR 工具链
└── platforms/
    ├── Kconfig                # 平台配置分发
    ├── native/Kconfig         # Native 平台配置
    └── stm32/Kconfig          # STM32 平台配置
```

## 配置更新流程

当修改 Kconfig 配置后：

```bash
# 1. 修改配置
make menuconfig

# 2. 重新配置 CMake (自动检测 .config 变化)
cmake -B build

# 3. 重新构建
cmake --build build
```

CMake 会自动检测 `.config` 文件的变化并重新配置。

## 调试配置问题

### 查看 Kconfig 变量

```bash
# 查看所有 CONFIG_* 变量
cmake -B build -LA | grep CONFIG_
```

### 查看 CMake 变量

```bash
# 查看所有 NEXUS_* 变量
cmake -B build -LA | grep NEXUS_
```

### 详细输出

```bash
# 启用详细输出
cmake -B build --trace-expand
```

### 验证配置

```bash
# 查看配置摘要
cmake -B build 2>&1 | grep -A 20 "Nexus Build Configuration"
```

## 常见问题

### Q1: 修改 Kconfig 后 CMake 没有更新？

**A**: CMake 会自动检测 `.config` 文件变化。如果没有更新，尝试：

```bash
# 删除 CMake 缓存
rm -rf build/CMakeCache.txt

# 重新配置
cmake -B build
```

### Q2: 工具链配置不生效？

**A**: 工具链文件必须在首次配置时指定：

```bash
# 错误：在已配置的构建目录中更改工具链
cmake -B build -DCMAKE_TOOLCHAIN_FILE=...  # 不会生效

# 正确：删除构建目录或使用新目录
rm -rf build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=...
```

### Q3: 如何覆盖 Kconfig 配置？

**A**: 使用 CMake 命令行参数：

```bash
# 覆盖平台配置
cmake -B build -DNEXUS_PLATFORM=stm32

# 覆盖构建类型
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

### Q4: 如何查看当前配置？

**A**: 查看构建摘要：

```bash
cmake -B build 2>&1 | tail -30
```

## 最佳实践

1. **使用 Kconfig 进行配置**: 优先使用 menuconfig 配置项目，而不是直接修改 CMake 变量

2. **提交 .config 文件**: 将 `.config` 文件提交到版本控制，确保团队使用相同配置

3. **使用 defconfig**: 为不同配置创建 defconfig 文件：
   ```bash
   # 保存当前配置为 defconfig
   make savedefconfig
   
   # 加载 defconfig
   make defconfig DEFCONFIG=platforms/stm32/defconfig_stm32f4
   ```

4. **使用 CMake 预设**: 为常用配置创建 CMake 预设，简化配置流程

5. **验证配置**: 构建前检查配置摘要，确保配置正确

## 参考资料

- [Kconfig 配置系统设计](kconfig-design.md)
- [CMake 工具链文件](../cmake/toolchains/)
- [平台配置文档](../platforms/)
