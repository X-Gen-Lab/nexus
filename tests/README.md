# Nexus 测试框架

本目录包含 Nexus 项目的所有 Google Test 测试套件。

## 目录结构

```
tests/
├── CMakeLists.txt              # 主测试配置文件
├── test_main.cpp               # HAL 测试入口点（含设备设置）
├── test_main_common.cpp        # 通用测试入口点
├── hal/                        # HAL 层测试
│   ├── CMakeLists.txt
│   └── native/                 # Native 平台测试
│       ├── CMakeLists.txt
│       ├── devices/            # 设备模拟器
│       └── test_*.cpp          # 测试文件
├── osal/                       # OSAL 测试
│   ├── CMakeLists.txt
│   └── test_*.cpp
├── config/                     # Config 框架测试
│   ├── CMakeLists.txt
│   └── test_*.cpp
├── log/                        # Log 框架测试
│   ├── CMakeLists.txt
│   └── test_*.cpp
├── shell/                      # Shell 框架测试
│   ├── CMakeLists.txt
│   └── test_*.cpp
├── init/                       # Init 框架测试
│   ├── CMakeLists.txt
│   └── test_*.cpp
└── integration/                # 集成测试
    ├── CMakeLists.txt
    └── test_*.cpp
```

## 构建系统设计

### 模块化设计

每个测试模块都有自己的 CMakeLists.txt 文件，创建独立的测试可执行文件：

- `hal_native_tests` - HAL Native 平台测试（使用 test_main.cpp）
- `osal_tests` - OSAL 测试（使用 test_main_common.cpp）
- `config_tests` - Config 框架测试（使用 test_main_common.cpp）
- `log_tests` - Log 框架测试（使用 test_main_common.cpp）
- `shell_tests` - Shell 框架测试（使用 test_main_common.cpp）
- `init_tests` - Init 框架测试（使用 test_main_common.cpp）
- `integration_tests` - 集成测试（使用 test_main_common.cpp）

### 测试入口点

- **test_main.cpp**: HAL 测试专用，包含 MSVC 平台的设备初始化和清理
- **test_main_common.cpp**: 通用测试入口，不依赖平台特定的设备设置

### 共享配置

主 `tests/CMakeLists.txt` 定义了所有测试共享的配置：

- `NEXUS_TEST_INCLUDES` - 公共头文件路径
- `NEXUS_TEST_LIBS` - 公共链接库
- `NEXUS_TEST_COMPILE_OPTIONS` - 公共编译选项

### 优势

1. **模块化** - 每个模块独立构建，易于维护
2. **并行构建** - 多个测试可执行文件可并行编译
3. **选择性构建** - 可以只构建需要的测试模块
4. **清晰的依赖** - 每个模块的依赖关系明确
5. **易于扩展** - 添加新测试模块只需创建新目录和 CMakeLists.txt
6. **独立运行** - 每个测试套件可以独立运行和调试

## 构建测试

### 构建所有测试

```bash
cmake -B build
cmake --build build
```

### 构建特定测试模块

```bash
cmake --build build --target hal_native_tests
cmake --build build --target osal_tests
cmake --build build --target config_tests
cmake --build build --target log_tests
cmake --build build --target shell_tests
cmake --build build --target init_tests
cmake --build build --target integration_tests
```

## 运行测试

### 运行所有测试

```bash
cd build
ctest -C Debug
```

### 运行特定测试模块

```bash
cd build
ctest -C Debug -R hal_native_tests
ctest -C Debug -R osal_tests
ctest -C Debug -R config_tests
```

### 运行特定标签的测试

```bash
ctest -C Debug -L unit          # 只运行单元测试
ctest -C Debug -L property      # 只运行属性测试
ctest -C Debug -L integration   # 只运行集成测试
ctest -C Debug -L hal           # 只运行 HAL 测试
ctest -C Debug -L framework     # 只运行框架测试
```

### 运行特定测试用例

```bash
ctest -C Debug -R "ConfigStoreTest.SetGetI32"
ctest -C Debug -R "OsalMutexTest.*"
```

### 并行运行测试

```bash
ctest -C Debug -j8              # 使用 8 个并行作业
```

### 详细输出

```bash
ctest -C Debug -V               # 详细输出
ctest -C Debug --output-on-failure  # 失败时输出
```

## 测试统计

当前测试套件包含：

- **总测试数**: 1539 个测试
- **HAL 测试**: ~400 个测试
- **OSAL 测试**: ~200 个测试
- **Config 测试**: ~300 个测试
- **Log 测试**: ~130 个测试
- **Shell 测试**: ~400 个测试
- **Init 测试**: ~15 个测试
- **Integration 测试**: ~40 个测试

## 添加新测试

### 1. 添加到现有模块

在相应目录的 CMakeLists.txt 中添加测试文件：

```cmake
set(OSAL_TEST_SOURCES
    # ... 现有文件 ...
    test_new_feature.cpp  # 新测试文件
)
```

### 2. 创建新测试模块

1. 创建新目录：`tests/new_module/`
2. 创建 `tests/new_module/CMakeLists.txt`：

```cmake
##############################################################################
# New Module Tests
##############################################################################

set(NEW_MODULE_TEST_SOURCES
    test_feature1.cpp
    test_feature2.cpp
)

add_executable(new_module_tests
    ${CMAKE_SOURCE_DIR}/tests/test_main_common.cpp
    ${NEW_MODULE_TEST_SOURCES}
)

target_compile_features(new_module_tests PRIVATE cxx_std_20)
target_compile_definitions(new_module_tests PRIVATE NX_STARTUP_TEST_MODE)
target_compile_options(new_module_tests PRIVATE ${NEXUS_TEST_COMPILE_OPTIONS})
target_include_directories(new_module_tests PRIVATE ${NEXUS_TEST_INCLUDES})
target_link_libraries(new_module_tests PRIVATE ${NEXUS_TEST_LIBS})

gtest_discover_tests(new_module_tests
    PROPERTIES
        TIMEOUT 300
        LABELS "new_module;unit"
)
```

3. 在 `tests/CMakeLists.txt` 中添加：

```cmake
add_subdirectory(new_module)
```

## CTest 配置

测试框架使用以下 CTest 配置：

- **并行级别**: 自动检测 CPU 核心数（当前：24）
- **超时时间**: 每个测试 300 秒（5 分钟）
- **失败输出**: 自动显示失败测试的输出
- **发现模式**: PRE_TEST（构建时发现测试）

## 测试标签

测试使用以下标签进行分类：

- `unit` - 单元测试
- `property` - 基于属性的测试
- `integration` - 集成测试
- `hal` - HAL 层测试
- `native` - Native 平台特定测试
- `osal` - OSAL 测试
- `framework` - 框架测试
- `config` - Config 框架测试
- `log` - Log 框架测试
- `shell` - Shell 框架测试
- `init` - Init 框架测试

## 注意事项

1. **测试入口点**：
   - HAL 测试使用 `test_main.cpp`（包含设备设置）
   - 其他测试使用 `test_main_common.cpp`（通用入口）

2. **C++20 支持**：所有测试需要 C++20（用于指定初始化器）

3. **测试模式宏**：所有测试定义 `NX_STARTUP_TEST_MODE` 宏

4. **MSVC 编译**：使用 `/FS` 标志支持并行编译

5. **Windows 平台**：在 Windows 上运行 CTest 时需要指定配置类型：
   ```bash
   ctest -C Debug
   ```

## 故障排除

### 测试发现失败

如果遇到测试发现错误，确保：
1. 所有测试可执行文件已成功构建
2. 使用 `-C Debug` 指定配置类型（Windows）
3. 重新运行 CMake 配置：`cmake -B build`

### 链接错误

如果遇到未定义符号错误：
1. 检查测试是否使用了正确的入口点（test_main.cpp vs test_main_common.cpp）
2. 确保所有依赖库都已链接
3. 验证头文件包含路径是否正确

### 编译错误

如果遇到编译错误：
1. 确保使用 C++20 编译器
2. 检查头文件路径是否正确
3. 验证所有源文件都已添加到 CMakeLists.txt

