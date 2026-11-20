---
id: guides.cpu-runtime
type: how-to
audience: consumer
source_language: zh_CN
---

# 只消费 CPU Runtime

仅复用 Core、Arch、公开 I/O 和所选 OS 时，选择 `Nexus::Runtime`。调用者自己提供
启动、向量、链接布局、时钟、Board 和具体驱动；此入口不创建 SoC factory。
完整 SoC／Board 工程继续使用 `nexus_add_platform`，两者在一个构建目录中互斥。

## 提供运行时事实

应用维护 `runtime.toml` 与 CPU 包的 `cpu.json`。最小 runtime 输入如下，路径相对
该 TOML；对应供应方须已按 SDK profile 准备：

```toml
schema_version = 1
cpu_facts = "cpu.json"
backend = "freertos"
optimization = "Os"
```

下面是 M33 NonSecure 软件配置示意；IRQ、时钟和可选能力须替换为实际审核事实，
示意不会增加新的芯片或 Board 支持：

```json
{
  "schema_version": 1,
  "cpu": {
    "arch": "cortex-m33", "dsp": false, "fpu": "none",
    "float_abi": "soft", "dwt_cyccnt": false, "mpu_version": 8,
    "icache_line_bytes": 0, "dcache_line_bytes": 0,
    "security": "nonsecure", "sau": false, "mve": "none"
  },
  "irq": {"priority_bits": 3, "external_count": 64},
  "clock_hz": 64000000
}
```

后端为 baremetal、FreeRTOS 或已维护范围内的 ThreadX；裸机不接纳内核策略，
CPU-only 输入也不接纳需要 SoC timer 事实的低功耗选择。`clock_hz` 描述频率，
不执行时钟配置。未知字段、矛盾特征／ABI 和非法路径必须拒绝。

## 显式创建 Runtime

以下是库消费片段，尚不构成可上板固件：

```cmake
cmake_minimum_required(VERSION 3.31)
project(runtime_consumer LANGUAGES C ASM)
find_package(Nexus CONFIG REQUIRED)
nexus_add_runtime(ASSEMBLY "${CMAKE_CURRENT_SOURCE_DIR}/runtime.toml")
add_library(application STATIC application.c)
target_link_libraries(application PRIVATE Nexus::Runtime)
```

MCU 自建 executable 还需调用 `nexus_apply_firmware_policy(target)`，并显式提供
自己的启动与链接脚本。仅连接 Runtime 不自动选择运行库或 `-nostartfiles`。
工具链仍须在 `project()` 前选定。

单独生成时可运行：

```sh
python tools/configure/runtime.py --assembly <runtime.toml> --output <owned-output>
```

输出 CPU resolved、配置头与选择信息，不输出设备
bindings／factory／链接脚本。完整字段和后端范围见
[Arch 参考](../reference/platforms/cortex-m.md)与[OS 合同](../reference/contracts/os/execution.md)。
