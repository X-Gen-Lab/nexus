# 当前证据与验收边界

本次静态 factory／多态接口／TOML／DMA block 迭代必须重新执行验证。旧版本候选、
历史测试数量和开发目录不能继承为新 HEAD 的资格。实施台账的 implemented
表示源码和工具已落地；验收状态必须读取对应 HEAD 的执行报告。最终执行保存在外部
`nexus-delivery/<HEAD>/`；每份报告绑定源码、配置、依赖、工具、命令返回码和产物。

| 范围 | 当前执行入口 | 资格边界 |
|---|---|---|
| Native／Core／OS／组件 | `tools/dev/dev.py test --preset native-debug`，另执行 release 和 ASan／UBSan | 实际软件所有权、线程、故障行为；不构成 MCU 电气资格 |
| GoogleTest／GoogleMock | `scripts/ci/tdd_gate.py --all`；`tests/google/` | 全量发现与实际非过滤执行、fresh XML、零跳过；不是历史编辑顺序的自动证明 |
| Cortex-M CPU 原语 | `tests/google/arch_*`；`scripts/ci/arch_compile.py` | 七个内核的真实对象／指令／原子依赖及生产 TU 模型；不扩大 SoC、RTOS port 或实板支持 |
| 生成器 | `tools/testing/run_tool_tests.py --suite configure` | TOML、冻结 IR、九类 Native factory、物理资源冲突和错误输入 |
| 工具门禁 | `tools/testing/run_tool_tests.py --suite testing`；`tests/contracts/test_*.py`；`scripts/ci/` | 实际负路径、暂存区稳定、报告身份和执行退出码 |
| STM／GD 寄存器模型 | `tests/google/*dma*`、`*uart_stream*`、`*adc_stream*`、`gd32_multi_test.cpp` 与已有 C contracts | 真实生产 TU；内存／线上排空、EN 拒绝关闭、迟到 IRQ、借用／隔离、校准／稳定 |
| 三板基础配置 | 六个 ARM presets | 实际 startup／vector／provider 链接、ELF／BIN／map 和预算 |
| 高级 controller 夹具 | `tests/contracts/{stm32,gd32}_assembly/*.toml` | 九类、DMA、块接收／触发、多控制器实际 ARM 链接；明确是软件 Board |
| 最小资源矩阵 | `tools/measurement/compare.py` | 三板 60 个 O2／Os／O3 ± LTO 镜像；18 empty GC 检查；实际 face／ops 存储成本 |
| 外部示例 | 独立 `X-Gen-Lab/nexus-examples` | 精确 pin 后 Native 运行与六个 ARM 配置；产品实现留在外部 |
| 源码 SDK | `cmake/package/package_source_sdk.py`、`verify_consumers.py` | 干净版本、移动路径 C／C++ Native 运行与 STM／GD 固件链接 |
| 离线复现／候选 | `tools/evidence/reproduce.py`、`candidate.py`、`proof.py` | 两次独立 network-none 构建，相同源码／配置／工具／ELF 身份及必需 scope |
| HIL | `tools/hil/prepare.py`、`admit.py`、三板 fixture 与 station template | 工装已准备；未连接设备，所有物理结果 `not_executed` |

开发执行目录 `build/controller-routes-factory/` 与
`build/minimal-factory-resources/` 保留独立 attempt 和原始命令，不覆盖旧运行。资源
数字只描述对应 ELF；源码变化后重新执行。MSP 显式预留，heap=0；未使用的内存域
不因芯片具有该区域就自动驻留。汇编和 ELF 字节不能替代实际 cycles／IRQ latency。

RED 日志保存在 `tools/testing/evidence/`，GREEN 为真实框架执行。既有正确合同的
迁移明确标识，不为了制造 RED 而修改生产行为。commit hook 开始和结束检查暂存
快照及 index tree，拒绝执行期间发生的行为变更；失败不发布新的成功摘要。

正式软件资格要求 clean source/dependency、准确工具环境与配置、非零 fresh tests、
同一 ELF／BIN／map、当前预算、源码 SDK 独立消费与离线双构建。支持范围以
[精确支持矩阵](support.md) 为准，不把软件夹具的 connector 继承到参考 Board。

所有物理时序、精度、波形、reset、掉电和长期负载项目尚未执行。实际 HIL 必须
记录 PCB、chip UID、probe／serial／power、station、预算和同一候选 image。
