# 首发板 HIL 工装准备与软件验收

关联 `RF-HIL-01/02`、`RF-CI-01`。本轮交付实验室准入、租约和记录工具，并未连接实板。模板的 `hardware_verified:false` 是固定边界；没有实物 PCB、探针、串口与受审 programmer adapter 时，准入必须失败。模型测试执行的是编排、拒绝与清理合同，不证明 MCU IRQ、DMA、电气、掉电或负载时间。

## 已实施的工具

| 工具 | 当前行为 |
|---|---|
| `scripts/hil/board_fixture.py` | 绑定 Board package/effective config、layout、静态报告、ELF/BIN、工装和 station；默认只准入/dry-run，`--execute` 才调用物理 adapter |
| `scripts/hil/run_hil.py` | 实物 Board、探针和串口三资源独占租约；依次 identify/program/readback/reset/serial；失败后 cleanup，清理失败保留隔离租约 |
| `scripts/hil/serial_echo.py` | 默认规划，不开串口；明确 `--execute` 后通过 pyserial 的独占串口发送随机公开挑战、读取完全回显；最多 8192 字节记录和有限超时 |
| `scripts/hil/fixtures/*.json` | 三首发板与 Discovery 参考接线模板；`fixture.schema.json` 给机器字段，实际校验不依赖额外 JSON Schema 库 |
| `tests/hil/test_board_fixture.py` | 注入模型 adapter/串口，硬件标记始终 false；验证身份错误、readback、零/跳过测试、缺失输入、tool hash、竞争租约和隔离 |

`station.template.json` 保留 null 身份和空 command，故意不能直接执行。将实验室真实数据填入平台仓库之外的受审 station 文件。未内置虚构的 probe serial、UID、PCB revision 或 GD32 OpenOCD target。

## 板卡与接线

Board 的编译接线唯一来源是相应 `boards/.../board.json`。HIL 准入重新验证这些文件及有效配置，检查 fixture 的 console 引脚和 controller 与实际 active resources 相同。以下只是参考接线；每块实物独立签收和记录 PCB/装配。

| 板卡 | MCU／Flash | Console 与主机连接 | LED | 调试与特别检查 |
|---|---|---|---|---|
| 启明欣欣高配 V3.1 | STM32F407ZGT6／1MiB | USART1：PA9 TX→主机 RX，PA10 RX←主机 TX，115200 8N1，共地 | PE3，低有效 | 记录 P14/jumper。MCU 引脚是 3.3V TTL；MAX232 后的 RS232 接口必须用 RS232 adapter |
| 天空星青春版 | STM32F407VET6／512KiB | USART1：PA9/PA10，3.3V TTL，共地 | PB2，高有效 | 青春版 SWD 接头可能未焊接；核对 LSE、外部 Flash、排针实际装配 |
| 梁山派 | GD32F470ZGT6／1MiB | USART0：PA9/PA10，3.3V TTL，共地 | PD7，高有效 | 核对 25MHz 晶振、实际 SWD 和串口连接；不能以 STM32 target 推断 GD32 Flash 算法 |
| Discovery MB997 参考 | STM32F407VGT6／1MiB | 当前 profile 使用 USART2：PA2/PA3，3.3V TTL，共地 | PD12，高有效 | 板载 ST-LINK 不等于串口桥；记录 ST-LINK 型号/序号、PCB revision 和 CN3 跳线 |

先断开工业负载和产品输出，记录 2.7–3.6V 范围内的实际供电及来源；常规参考连接为 3.3V。SWD 连接为目标 GND、VTref、SWDIO、SWCLK、必要时 NRST。探针不得向已经独立供电的板卡反向送电。适配电气路径是工装必要条件，不由软件报告代替。

## 实验室 adapter 与命令

当前提供受审外部 programmer command 接口；没有在未知 station 上执行 OpenOCD 或自动下载/安装 lab 工具。OpenOCD、ST-LINK、CMSIS-DAP 或厂商 programmer 的实际版本、可执行文件、interface/target 配置和 wrapper 都写入 `file_identity`（绝对真实路径、SHA256、size）。`backend` 使用 `openocd` 或 `external-reviewed`，`interface` 使用 `stlink`、`cmsis-dap` 或 `external-reviewed`。GD32 必须提供已审核、确实支持 F470 页擦除与芯片身份读取的 target/wrapper；模板不填猜测 target。

station 的六个 `commands` 都是 argv 数组，不使用 shell：

| 命令 | 必须实际产生的行为／观察 |
|---|---|
| `identify` | 观察 probe serial、设备 UID、芯片系列/容量；和实物 PCB/芯片丝印签收记录合并。输出 schema1 与完整 `board`。不能把 station 期望值原样称作芯片观察 |
| `flash` | 只写本次核准镜像的地址和字节，检查返回值；不默认 mass erase，不写 option bytes、密钥、计数器或外部存储 |
| `verify_flash` | 对同一 image extent 执行真实 readback，并计算读回字节的 SHA256；输出 `firmware_sha256`，不能只计算待写文件的 hash |
| `reset` | 真实 reset/run，记录复位方式；不能以 host 命令成功解释为固件已运行 |
| `serial` | 对 `uart_echo` 镜像执行公开随机字节挑战，保留真实 RX；将观察和刚完成的 probe/readback 身份关联后输出要求的 test IDs、配置和工件身份 |
| `cleanup` | 停止 station 子进程，settle 探针和线路，恢复已审安全初始连接；失败必须返回非零，由 runner 隔离三类租约 |

例如 STM32 的受审 OpenOCD wrapper 可使用 `interface/stlink.cfg` 或 `interface/cmsis-dap.cfg`、`target/stm32f4x.cfg` 和实际 probe serial。具体传输枚举（`swd`／工具版本要求的 `hla_swd`）、SWD 频率和 probe firmware 由 lab 审核；wrapper 应把下面的动作组成有界 argv/Tcl 调用：

```text
identify: init → halt → read DBGMCU/UID/Flash density → structured observed identity
flash:    program <the admitted BIN> <layout image origin> → verify success
readback: dump_image <temporary readback> <image origin> <exact BIN byte length> → SHA256
reset:    reset run
```

受审 programmer wrapper 必须约束输入为真实 reviewed paths，并正确转义工具自身的 Tcl/配置语言；`shell=False` 不能自动防止工具语言注入。F407 UID/容量读取和 GD32 身份读取遵循各自官方手册。封装号/PCB revision 无法仅靠 DBGMCU 推断，必须来自实物签收。

通用 UART 观察入口：

```sh
python scripts/hil/serial_echo.py --port /dev/serial/by-id/<actual-adapter> \
  --baudrate 115200 --timeout-s 5 --report /lab/evidence/uart-plan.json
# 真实、受控的连接和工具就绪后才显式 --execute
```

UART 观察不能独立证明板卡或固件身份；旧 UART echo banner 没有 SHA/UID 协议。station adapter 必须与刚完成的真实 Flash readback 关联，不能凭 banner 宣称镜像身份。

## 构建、准入与运行

1. 从固定 Nexus/examples SHA pair 在独立 build root 构建 `uart_echo`。输出有效配置、Board 输入 hash、layout、ELF/map/BIN 和 source/toolchain 记录。
2. 使用平台 ELF checker 生成 `arm-static-link-contract`；此时 `hardware_verified:false`。报告必须绑定 `board_id`、`board_sha256`、`layout_sha256`、`config_sha256` 和被选 ELF hash。
3. 准入重新校验 Board 实际文件与 inputs、layout canonical hash 和 erase boundary/重叠；检查 ELF 的 image/region/layout symbols；从 ELF file-backed allocated sections 重建 BIN，逐字节比对，防止拿别的 BIN 刷写。
4. 签收实物 station：实际 inventory ID、PCB revision、芯片丝印、96-bit UID、probe serial、串口设备和电气接线。锁定 reviewed tools/adapter/config 文件 hash。缺失项直接失败。
5. 先 dry-run，查看具体核准镜像、Board/layout/config 身份与计划动作。它不获取探针，也不写板。
6. 受控 lab 人员显式执行。Board inventory、物理 probe 和 canonical tty 三类资源在同一共享 lease root 独占；identify 错误在编程前停止；readback 错误在 reset/UART 前停止。
7. 本轮 smoke 只认 `probe_identity`、`flash_readback`、`reset`、`uart_echo` 全部 pass。零、缺失、重复、fail/skipped 和预算错误都失败；清理失败隔离，不能依 TTL 自动抢占。

```sh
python scripts/ci/validate_firmware_elf.py --build-dir /lab/build/nexus \
  --report /lab/evidence/static.json
python scripts/hil/board_fixture.py \
  --fixture scripts/hil/fixtures/stm32f407_qiming_v31.json \
  --station /lab/stations/qiming-actual.json --build-dir /lab/build/nexus \
  --image uart_echo --static-report /lab/evidence/static.json \
  --leases /lab/shared-leases --report /lab/evidence/qiming-plan.json
# 上一步只准入。真实执行使用另一个report路径，并显式追加 --execute。
```

runner 记录 UTC 时间、fixture/station/build 身份、程序与 readback/UART 的实际步骤、RX transcript digest 和清理状态。面向用户的排期用 Asia/Shanghai；记录保留 UTC，方便跨站比较。

软件模型只能通过 `model=True` 运行注入依赖；CLI 没有把 board fixture model 伪装成物理运行的开关。注入 artifact validator、adapter 或串口时必须显式 model，所有报告 `hardware_verified:false`、`eligible_physical_hil:false`。正式发布仍需要组织信任、公钥/签名、真实 HIL 与独立 review；本工具的 JSON 标签不是设备/实验室的密码学证明。

## 仍待实测的资格项目

fixture 为 IRQ 抢占/优先级、UART wire-TC/取消、SPI/DMA/CS、Flash 断电和控制 jitter 提供接口说明与 `budget:null`，不得填假测值。真实产品工作负载和预算必须由外部示例/产品仓库冻结。

- IRQ：实际高 IRQ、嵌套/优先级、调度前后 mask 恢复、最坏延迟。
- UART：AF 输出、线速、TC 与 DMA/取消竞争、缓冲结清、过载与错误事件。
- SPI/DMA：真实 slave/loopback、CS 建立/保持、DMA 可达区域、失败与超时线路结清。
- Flash：外部明确 layout 的可擦区域、供电条件、实际密度、write protection、DWT/TIMER1 deadline 和 single-bank 暂停。擦写/断电测试单独审查授权，不由 smoke runner 自动添加。
- 压力：真实控制周期、通信/日志/Flash 负载、jitter、栈/池和长期 token 预算。

缺设备时停在“工装就绪／物理待测”，不能把 host fake、ARM 静态通过或模板字段提升为硬件支持。

## 本轮软件验证

2026-10-09 执行以下软件检查，未执行实板命令：

```sh
python3 -m unittest discover -s tests/hil -p 'test_*.py' -v
python3 -m unittest scripts.evidence.test_enterprise_tools.AdapterFixture -v
```

新增 Board/串口模型 23/23、既有 HIL 编排回归 20/20 通过，均没有跳过。对当前 `stm32-qiming-armgcc-freertos-release` 的 `nexus_contract_firmware` 执行真实静态 checker 和 `validate_build`，确认 Board/layout SHA 符号、有效配置、ELF/BIN 字节及 11 个工件身份一致。复制这些工件后，BIN、ELF、有效配置、Board identity、layout identity 和静态报告 Board SHA 六种篡改全部被拒绝。软件记录为 `build/rf-hil-artifact-admission.json`；它明确 `hardware_verified:false`、`eligible_physical_hil:false`。该镜像是平台链接合同测试镜像，未作为 `uart_echo` 或实板运行资格使用。

历史 ELF/BIN 文件只用于检查纯重建算法的字节对应，不计入新平台镜像或硬件资格。正式资格应在最终提交及 Nexus/examples SHA pair 上重新绑定构建、静态报告和实验室记录；本段计数不能代替那个最终记录。
