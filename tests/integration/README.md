# 平台集成合同

本目录验证独立工程、配置生成、CPU／ABI、真实内核、目标链接和安装 SDK 的组合。
测试消费生产 CMake 图与实际平台实现，场景输入和可观察结果由各叶目录说明。

| 目录 | 维护内容 |
| --- | --- |
| `c_api/` | 真实 C11 公共接口消费、原子与生命周期组合 |
| `arch/` | CPU runtime、布局和 ABI 编译／链接探针 |
| `kernels/` | 内核端口、上下文、MPU、安全域与低功耗组合 |
| `assemblies/` | TOML、Board、静态实例、factory 与精确外设模式 |
| `sdk/` | 来源选择、导出、搬迁及独立公开消费者 |
| `protocols/` | 网络对端、TLS／MQTT、持久化和升级镜像 |
| `providers/` | 厂商库比较、寄存器模型与外置包接入 |
| `resources/` | 静态尺寸、内存区域、ELF 预算、报告及 HIL 工装准入 |

Native 构建将独立 C11 程序与行为测试一起注册到完整 CTest 图。真实 FreeRTOS
POSIX port 和 STM32 模型有各自执行目标；仅配置输入和独立 ARM 工程由工具域及
相应叶入口驱动。读取每个场景的源码、编译宏与 README 确定其验证范围。

在仓库根、锁定工具与选定依赖准备完成后，配置并构建 Native 目标，再执行完整
运行期计划：

```sh
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug
python scripts/ci/native_contracts.py --preset native-debug
```

Python 集成合同使用现有工具执行器：

```sh
python tools/testing/run_tool_tests.py --suite integration \
  --report build/quality/tools-integration.json
```

SDK 携带的规范消费者由 `cmake/package/verification` 与包导出声明维护。这里编排
并验证同一份输入。外部应用示例由 `nexus-examples` 维护其装配和产品组合。

`providers/declarative_gd32f470` 是显式跨厂家软件参考，当前生产 SoC 范围为
STM32F4。主机模型观察软件行为，ARM ELF 观察编译、链接和资源，物理 HIL 需要
绑定实际 PCB、station 与镜像。每次保留实际命令、返回码和对应产物。
