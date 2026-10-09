# 当前证据与验收边界

此文记录本轮开发执行入口。开发工作区、手工执行或历史日志不能自动成为当前
clean-source 候选证据；最终资格文件必须由工具以准确 source/config/tool/ELF 身份
和 fresh raw execution 生成。构建日志路径是本轮环境中的证据定位，不承诺所有
临时开发产物都会随源码包发布。

| 范围 | 实际执行／证据入口 | 资格边界 |
|---|---|---|
| Core、Native I/O | `tests/contracts/core_contract_test.c`、`native_io_test.c`；GCC14.2 strict 与 ASan/UBSan 开发执行 | request 发布、deadline/wrap、RX/EXTI loss ordering 等软件行为 |
| STM32 | `tests/contracts/stm32_model/`、`stm32_io_test.c`；真实生产 TU；独立 VE/ZG controller ARM fixtures | 模型与编译／链接，不是 PCB 电气 |
| GD32 | `tests/contracts/gd32_io_test.c`、`gd32_system_test.c`；四个 CTest；`build/native-asan` 实际通过 | TC/late IRQ、quarantine、mask/clock rollback、watchdog 不可逆失败；DBG 模型地址为 ASan 重定位，生产 SDK 不变 |
| GD32 ARM controllers | `build/gd32-controller-{spi,i2c,flash,watchdog,exti,pwm,adc,adc-scan}` 真实链接 | 8 个独立软件 Board fixture，非 HIL 镜像 |
| OS／通用组件 | `build/ng-agent-cmake/os-components-junit.xml` 10/10、`build/ng-agent-sanitized/os-components-junit.xml` 9/9 | 实际 POSIX/FreeRTOS POSIX、3 producer/3,000 帧 UART owner、并发／掉电模型；FreeRTOS POSIX cancellation 修后 100 fresh process 通过 |
| 三板资源矩阵 | `build/measurement-qiming-v3/matrix.json` 18、`measurement-sky-v1/matrix.json` 18、`measurement-liangshan-v5/matrix.json` 24 | 60 个 O2/Os/O3±LTO 开发镜像；不是 cycles 或最终封存 candidate |
| GC 负路径 | `build/measurement-qiming-gc-v2`、`measurement-sky-gc-v2`、`measurement-liangshan-gc` | weak IRQ alias 不冒充真实 provider，未选 provider 不驻留 |
| 外部示例 | `nexus-examples/build/nextgen-development/` 的 Native JUnit、ARM development JSON、script log | 实际开发消费；最终 pin/clean SDK 需要独立验证 |
| 工具边界 | `tests/contracts/test_{measurement,evidence,hil,repro,source_sdk}*.py` 与配置测试 | 缺失／失效／错 scope／伪造／空报告真实拒绝；模型不代替正式操作 |
| OCI | 本轮实际 seal/verify/load 的 manifest digest `e936b188b9e30cf37ce109a5407019c91fe2dbc62fb10afceb3f2abbfc823ac8` | 本地观测 OCI；非发布 registry，Dockerfile recipe 不声称可复现 |
| 离线双构建／候选 | `tools/evidence/reproduce.py`、`candidate.py`、`proof.py` | 工具已实现；以 clean HEAD 后正式输出为准，不能用开发 hash 代替 |
| HIL | `tools/hil/admit.py`、三 Board fixture、station template、准入负测 | 工装准备；未连接 station，物理 `not_executed` |

开发资源比较中的 Os 数值仅用于观察当前机制成本，MSP 都显式保留 2048 bytes、
heap=0，未启用的 CCM/TCM/ADDSRAM 占用为 0。后续源码改变会使 image hash 和
资源数值变化，不能把这一组开发数字复制成正式预算通过记录。

正式软件验收要求：clean source/dependency、准确工具环境与配置、非零 fresh tests、
实际 ELF/BIN/map bytes、当前预算、可移动源码 SDK 消费、两次独立 network-none
构建比较。candidate 验证同时检查 qualification 的 scope 和相同 artifact 身份。
报告字段、schema、文件摘要和 workflow 定义本身均不是执行证据。

所有物理时序／精度／波形／reset／掉电／负载项目尚未执行。实板资格必须绑定
PCB、chip UID、probe/serial/power、station identity、预算和同一候选 image，不能
继承另一 Board、软件 fixture 或旧版本的结果。
