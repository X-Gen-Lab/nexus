# 构建目标与静态固件组合

CMake 维护唯一源码和链接依赖图。`NEXUS_ASSEMBLY_FILE` 选择 TOML 装配，生成结果
只提供严格解析的选择、静态连接、布局与输入身份，不维护另一套源码列表。

当前生产 provider 为 `nexus_soc_stm32f4`，具体实现限定 F407VE／ZG。精确器件宏与
startup 从型号事实导出；官方 HAL／LL、Device 和 Handle 保持私有。未选模块不参与
编译，section GC 再移除未引用函数，两者分别验证。

外部工程自身提供 `main.c` 与 `assembly.toml`：

```cmake
cmake_minimum_required(VERSION 3.31)
project(firmware C ASM)
set(NEXUS_ASSEMBLY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assembly.toml")
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "" FORCE)
add_subdirectory(external/nexus nexus-owned)
nexus_add_firmware(firmware SOURCES main.c LIBRARIES Nexus::Log)
```

固件入口装入精确 startup、system、linker 和 ABI，不启动 scheduler 或产品 worker。
应用负责任务、stack／TCB、队列、Flash 区域与停机顺序。

| 位置 | 责任 |
|---|---|
| `platform/` | 装配、目标策略、ABI、固件与 CPU runtime |
| `package/` | 可移位源码 SDK、导出和实际消费验证 |
| `presets/` | F4 工程与显式主机测试 preset |
| `toolchains/` | 编译器选择；CPU 事实仍由严格配置决定 |

全部 vendor 留在仓库，默认构建只准备并验证选中依赖，不要求其他 SDK 已初始化。
`CMSISCore.cmake` 管理唯一锁定 Core include；产品不能用另一份 bundled Core 覆盖。
Native 只在 `NEXUS_TEST_SUPPORT=ON` 时从 `tests/support/native/` 载入，不接受生产
默认回退到主机模型。

Source SDK 是源码快照，按已选闭包提供实现和精确可选 host-support，不打包整个
`tests/` 或所有 vendor。完整源码模式命令、移动验证与资格见
[SDK 教程](../docs/tutorials/source-sdk.md)；当前结构见
[架构](../docs/delivery/architecture.md)。
