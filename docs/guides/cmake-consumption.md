---
id: guides.cmake-consumption
type: how-to
audience: consumer
source_language: zh_CN
---

# 用 CMake 接入平台

一个应用维护自己的 CMake 工程和 `assembly.toml`。源码检出和安装 Source SDK
使用相同接口；先选择工具链，再创建平台，最后创建镜像。

## 三个公开入口

```cmake
cmake_minimum_required(VERSION 3.31)
project(controller LANGUAGES C ASM)

find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(ASSEMBLY "${CMAKE_CURRENT_SOURCE_DIR}/assembly.toml")
nexus_add_firmware(controller SOURCES src/main.c)
```

| 调用 | 责任 |
| --- | --- |
| `find_package` | 检查所选来源，加载公开函数；不解析装配、不创建生产目标 |
| `nexus_add_platform` | 解析一个 TOML，生成静态实例，创建所选平台目标 |
| `nexus_add_firmware` | 为镜像连接启动对象、链接布局、产物及资源检查 |

工具链必须在 `project()` 前通过 preset 或命令行选定。每个构建目录只有一个
工具链和装配；不同板卡、后端或 SDK 使用独立目录。同一装配可创建多个镜像。

## 选择源码或安装 SDK

源码检出的 `Nexus_DIR` 为 `<nexus>/cmake`，安装 SDK 为
`<prefix>/lib/cmake/Nexus`。以 ARM 源码工程为例，从应用目录执行：

```sh
cmake -S . -B build/f407 -G Ninja \
  -DNexus_DIR="$NEXUS_SOURCE/cmake" \
  -DCMAKE_TOOLCHAIN_FILE="$NEXUS_SOURCE/cmake/toolchains/arm-gcc.cmake"
cmake --build build/f407 --parallel 2
```

`NEXUS_SOURCE` 在这里是命令示例中的路径变量。平台公开选择入口为 `Nexus_DIR`，
不要求应用设置平台开发测试或 workload 开关。普通配置不下载依赖；先按
[源码准备](source-checkout.md)完成所选任务的准备。

## 连接应用库与组件

下面的示例要求装配已选择 `log`；用这些库和镜像声明替换前面的镜像声明：

```cmake
add_library(control STATIC control.c)
target_link_libraries(control PRIVATE Nexus::Platform)
nexus_add_firmware(controller SOURCES main.c LIBRARIES control Nexus::Log)
```

`Nexus::Platform` 提供所选公开接口及生成头的使用要求。组件负责自己的公开和私有
依赖，应用不添加厂商 SDK 或 provider 私有目录。只需要 ABI 的自建目标可连接
`Nexus::ABI`；已选 OS 后端通过 `Nexus::OS` 连接，不拼接内部目标名。

## 独立 CPU 工程

只消费 Core、Arch、公共 I/O 和 OS 的独立 CPU 工程使用
[CPU Runtime 接入](cpu-runtime.md)。发现与装配合同及失败后的产物责任见
[构建合同](../explanation/build-model.md)。
