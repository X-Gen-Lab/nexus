# HIL 准备、采集与准入

本目录将精确配置、真实链接产物和站点观察关联，提供计划、准入、显式采集及原始
记录校验。工具定义不证明设备已连接或物理能力已通过；当前未执行案例保持
`not_executed`。产品拥有电气限制、接线、Flash 试验区域、真实负载和发布决定。

## 工具关系与准备

`prepare.py` 将生成的 `resolved.json` 和维护 fixture 转为未绑定计划，验证精确
part 与 smoke console pins，保留 selected controller modes。每个物理 case 均为
`not_executed`，不建立 station readiness 或 hardware qualification。

`admit.py` 在设备操作前检查 observed station、精确 ELF、配置和重算资源预算，
也按同一 admission 校验 raw physical records。模型、复制报告、空执行、变化 ELF
或 stale record 不认证硬件。四项 smoke 只验证记录范围；DMA、IRQ、stream、ADC、
cancel、timing、power-loss 须各自物理测量。

```text
配置／fixture → 未绑定计划
实际 station／ELF／资源 → admission
显式受审 acquisition → 原始观察（executed_unqualified）
原始 smoke／measure／OS／SoC 记录 → 各自 scope 的验证
```

准备、准入和校验不操作探针、不供电、不刷写、不打开串口；只有另外明确的 station
acquisition execute 才操作设备。软件证据与物理证据的边界可参照
[FIG-11](../../docs/assets/diagrams/qualification-evidence.svg)，图不表示已有测量。

先准备并配置同一受维护 preset，取得真实生成的 `resolved.json`。以下示例从仓库根
执行，不运行固件或操作设备；`configure` 要求所选源码及锁定工具已就绪：

```sh
python tools/dev/dev.py prepare --scope preset \
    --preset stm32-qiming-baremetal --allow-network
python tools/dev/dev.py configure --preset stm32-qiming-baremetal
python tools/hil/prepare.py \
    --fixture tools/hil/fixtures/stm32f407_qiming_v31.json \
    --resolved build/stm32-qiming-baremetal/generated/resolved.json \
    --report build/stm32-qiming-baremetal/hil-preparation.json
```

从源码仓库根执行计划命令。设备接入后，将 `fixtures/station.template.json` 复制
到 station 自有目录，实际观察 part、PCB revision、chip UID、probe serial、稳定
serial path、wiring、ground、supply，并给真实 flash tool／probe configs 记录内容
摘要。按真实负载审核数字 budgets，null budget 不能跳过评审。

若 resolved build 含 Board hardware options，准备保留精确 catalogue 和显式选择，
并标 `physical_verification=not_established_by_configuration`。缺省选择仍 unknown；
resolved-file identity 绑定声明，不创造第二个配置／物理观察权威。站点须独立检查
population、jumpers、connector personality 和 PCB revision，复制声明不等于检查。

## Fixture 与精确模式边界

当前维护四份 F407 fixture，均未取得实板观察。文件名、Board 和精确 part 必须与
同一构建的 `resolved.json` 一致，不能交换核心板、底板、V3.1 或天空星的身份。

| Fixture | 精确 part | 当前准入边界 |
| --- | --- | --- |
| `fixtures/stm32f407_qiming_v31.json` | STM32F407ZGT6 | 等待真实 PCB／station／预算和镜像绑定 |
| `fixtures/stm32f407ve_sky_qingchun.json` | STM32F407VET6 | 等待真实 PCB／station／预算和镜像绑定 |
| `fixtures/stm32f407zg_qiming_core.json` | STM32F407ZGT6 | `software_reference`，物理准入前须另审 fixture／tooling |
| `fixtures/stm32f407zg_qiming_v41.json` | STM32F407ZGT6 | `software_reference`，物理准入前须另审 fixture／tooling |

`admit.py` 维护精确 Board／part 对及 reference 限制。编辑 scope、part 或重算文件
hash 都不能升级资格；未审核的 revision、器件实装、跳线、电压和晶振精度保持未知。
四份 fixture 的物理案例均为 `not_executed`，null budgets 不能绕过实测条件审核。
ILI9341／XPT2046 型号确认不代表模块安装、电气时序或实物操作已观察。

HIL 的原始 trace、测量和 OS 记录分别准入。某个 packet／timer／DMA 模型通过，
不提供设备 ready、PHY／cache coherence、波形、唤醒或掉电证明。已退出生产选择的
GD32、G474、H743 和 U575 历史记录按原固定源码读取，不用于当前 F407 工装。

## 通用测量与前后比较

`measure.py plan` 绑定 fixture／resolved build 生成七项通用 cases，不需 station，
全部 budgets null，状态总为 `prepared_unbound`／`not_executed`。

```sh
python tools/hil/measure.py plan \
    --fixture tools/hil/fixtures/stm32f407_qiming_v31.json \
    --resolved build/stm32-qiming-baremetal/generated/resolved.json \
    --report build/stm32-qiming-baremetal/hil-measurement-plan.json
```

设备可用后 station owner 审核 workload／numeric budgets，通过
`admit.py --station ... --fixture ... --elf ... --resolved ... --resources ... --report admission.json`
生成新 admission。budget 需 `maximum`／`unit`，可选 `minimum` 默认零。throughput／
endurance 不能仅靠最大值证明足够工作量；例如
`throughput: {minimum: 1000, maximum: 1000000, unit: bytes_per_s}` 或
`duration: {minimum: 3600, maximum: 7200, unit: s}` 仅示意 schema，产品审实际 limits。
boolean、negative、nonfinite、reversed、oversized bounds 均拒绝。

设备 adapter 保留独立 JSON traces 和 run record，再校验：

```sh
python tools/hil/measure.py validate \
    --admission station/admission.json \
    --raw-evidence station/measurements.json \
    --report station/measurement-validation.json
```

优化前后复用同一 raw／admission 合同，两边全重验，旧 validation summary 不能代替
原件；report destination 必须不存在。

```sh
python tools/hil/measure.py compare \
    --baseline-raw station/before/measurements.json \
    --baseline-admission station/before/admission.json \
    --candidate-raw station/after/measurements.json \
    --candidate-admission station/after/admission.json \
    --report station/comparison.json
```

比较要求同 station／fixture bytes、Board／part／UID、reviewed budgets、resolved
runtime config、workload bytes、CPU rate、load、executor、IRQ priorities、采集
method／instrument／resolution／window，且每个 metric sample count 相同。复用
linked-cost observer 的 config projection；source provenance／ELF 可变，但 routes、
modes、memory regions、stack／heap reservations 须等价。不声称 compiler flags 或
implementation bytes 相等。差异条件按组报告，不同 workload／chip 是独立实验。

DWT 两 run 可有各自正确拥有的新 counter epochs，均须通过独立 reference／wrap
检查；capture clock role、instrument identity、rate、uncertainty、CPU frequency
envelope 相同，各 run 的原 timestamp／epoch 保留。两边默认 freshness 86400 秒，
更早基线须显式审核并设置 `--maximum-age-s`，不静默豁免。

结果为 `compared`／`comparison_only`，保留各 run stats 及 candidate-minus-baseline
的 signed minimum／maximum／mean 差量。不决定优化方向、不推断置信度或 WCET，
双方必须先满足各自 reviewed budgets。合成测试不认证 Board，未执行基线仍未执行。

## 测量记录 schema 2

run 为 strict schema-2 `physical_hil_measurements`：`started_utc`、`finished_utc`
（须 UTC offset）、`station_id`、`board`、`part`、`chip_uid`、`elf_sha256`、
`resolved_sha256`、`admission_sha256`、`context`、`traces`。admission digest 来自
`tools.evidence.identity.canonical_digest`。context／trace 的文件 identity 都是
`{path, sha256, size}`，指向保留的非空 regular bytes，变化即失效。

context 是 strict schema-2 `hil_acquisition_context`：

| 字段 | 必需观察 |
| --- | --- |
| `admission_sha256` | 不变 admission 的 canonical digest |
| `workload` | 实际负载与参数的文件 identity |
| `cpu_clock_hz` | 正数声明频率；cycle 测量另核对准入 envelope |
| `load` | 负荷／干扰，明确未测条件 |
| `executor` | owner／service 执行者、priority 与调度条件 |
| `irq_priorities` | 命名逻辑 IRQ priority，整数 0..255；IRQ latency 不得为空 |
| `measurements` | 与 station reviewed budgets 选择的 metrics 精确相同 |

每项 measurements 有 `method`、`instrument`（实际 config／calibration 文件身份）、
正数 `resolution`（该 metric 单位）和正数 `observation_window_s`；适用时还需下表
字段。每 trace 独立绑定 context hash，不能换 workload／条件复用。

| Metric | 单位 | 维护的方法 | 额外条件 |
| --- | --- | --- | --- |
| `cycles` | `cycles` | `dwt_snapshot` | `counter_bits: 32`、非零 `cycle_domain`／`cycle_epoch` 和独立 `reference` |
| `irq_latency` | `us` | `logic_analyzer`、`hardware_timer` | 实际 IRQ priorities、clock／load、capture resolution |
| `service_interval` | `us` | `monotonic_timestamp` | 所需 owner／service 保持运行，保留调度条件 |
| `stack_used_peak` | `bytes` | `freertos_high_water`、`stack_watermark` | 正整数 `stack_capacity_bytes`，先将 RTOS words 转 bytes |
| `throughput` | `bytes_per_s` | `wire_byte_counter` | 统计真实 wire bytes，产品明确 payload／overhead |
| `loss` | `count` | `receiver_event_counter` | 正整数 `expected_events`，保留序列／丢失统计 |
| `duration` | `s` | `monotonic_timestamp` | 真实 elapsed workload 与单调 clock calibration |

trace 为 strict schema-2 `physical_hil_trace`，含 `metric`、`unit`、`method`、
`admission_sha256`、`context_sha256`、`samples`。非 cycle 标量是 `{sequence, value}`，
从零连续，value finite／非负／不超过 unsigned 64-bit；stack／loss 是整数并不超过
context bound。每 trace 1..100000 samples，每 run 1..10 个唯一维护 metrics，
精确匹配 budgets。validator 从原 samples 算 min／max／mean／count，不信 supplied
stats，每个 sample 都满足上下限。

### 配对电压／电流的功耗与能量

默认七项不变，显式 `measure.py plan --power` 加 `power_mean`（`W`）、`power_peak`
（`W`）、`energy`（`J`）。仍是 null budgets 的 unbound checklist／not_executed，
实际 adapter／测量仍需 station／workload admission。

复用 schema-2 context／trace／validate／compare，每 metric 选
`method=paired_voltage_current`，instrument 绑定 calibration JSON，保留常规
resolution／observation_window_s，并增加 `power` 条件：

| 字段 | 合同 |
| --- | --- |
| `supply_domain`、`included_loads[]` | 显式供电域、非空且唯一的 load identities |
| `probe_influence` | probe、UART／USB、board loads 等影响 |
| `anti_alias_filter`、`bandwidth_hz` | 真实滤波／带宽，带宽不超过采样 Nyquist |
| `sample_period_ns`、`interval_tolerance_ns` | 正数周期和有界间隔误差 |
| `maximum_pair_skew_ns` | 有界 V／I timestamp 差 |
| `window_start_ns`、`window_end_ns` | 非负、严格有序，包含整数个 sample periods |

instrument JSON 为 schema 1 `physical_hil_power_calibration`，包含 `station_id`、
`instrument_id`、`calibration_id`、`calibrated_utc`、`valid_until_utc`、`timebase`、
`voltage`、`current`；有效期覆盖真实 run。timebase 为 `unit=ns`、正数
`resolution_ns` 和非负、低于一百万的 `accuracy_ppm`，timing tolerance 不得声称比
instrument 更细。每 channel 有 `unit`、正数 `gain`、`offset`、`minimum`、`maximum`
和正数 input-unit `resolution`；voltage 允许 `V`／`mV`，current 允许 `A`／`mA`／
`uA`，raw samples 在声明范围。SI 换算是 `(raw * gain + offset) * unit_scale`；
合同支持有界非负 consumption，不支持有符号 regenerative energy。

power trace samples 为 `{sequence, capture}`，capture 是保留文件 identity
（`path`、`sha256`、`size`），不是 summary。capture 为 schema 1
`physical_hil_power_capture`，含当前 admission_sha256／context_sha256、
`dropped_samples=0`、`overrange=false` 和 samples。每点含 `sequence`、
`voltage_time_ns`、`current_time_ns`、`voltage`、`current`：sequence 从零连续，
V timestamps 按审核 cadence 严格递增，V／I skew 有界，两路 timestamps 全在窗口，
首末 V 精确覆盖 endpoints。数量由窗口／cadence 派生，缺样、重复、乱序、超量程或
丢样拒绝。window duration 匹配 observation_window_s 且适合 run duration；instrument
纳秒 endpoints 不是独立验证的 UTC anchor。

每 metric 最多 128 captures 和 100000 aggregate paired points。同 metric 内不可
通过不同路径重复相同 SHA-256／size 的 raw bytes；同 capture 可分别派生 mean、
sampled peak 和 energy。

validator 对校准后的每对 V×I 作 trapezoidal rule 积分。power_mean 为能量除窗口
时长，power_peak 是最大采样乘积，energy 为焦耳积分；保留 raw／calibration identity、
count、duration 和 integration method。预算针对派生观察，calibration 声明未独立
认证，不承诺传播后 uncertainty 或点间 peak bound。按真实设备／负载选择 sample
rate、bandwidth、range 与工程余量。

使用既有 validate／compare；比较重验 raw，保持 supply、loads、instrument／
calibration、window、cadence 相同。失败 report 不覆盖嵌套 raw capture。工具回归
是 synthetic，不建立物理 power／energy／timing 结果。

### Cycle 的独立参考与回绕证明

cycle sample 为 `{sequence, begin, end}`，拒绝 supplied `value`。每 endpoint 精确
含 `counter`、`cycle_domain`、`cycle_epoch`、`cpu_clock_hz`、`demcr`、`dwt_control`、
`reference`，counter／register 为 unsigned 32-bit。两端须显示 TRCENA／CYCCNTENA、
无 NOCYCCNT，domain／epoch／声明频率与 context 不变。声明频率不是晶振精确测量；
真实 counter owner 在 reset、withdrawal、clock change 时提升 epoch，probe 不提供
owner，也不制造 epoch。

cycle context reference 有 `domain`、`epoch`、`ticks_per_second`、
`method: "logic_analyzer"` 和 instrument identity；仪器文件为保留的 schema-1
`physical_hil_reference_clock`：

| 字段 | 必需事实 |
| --- | --- |
| `admission_sha256` | 当前 admission digest |
| `role`／`clock_source` | `reference_capture`／`external_instrument_oscillator` |
| `instrument_id` | 真实外部采集设备身份 |
| `domain`、`epoch`、`ticks_per_second` | 非零 reference identity／rate，counter 与 reference domains 不同 |
| `cycle_domain`、`cycle_epoch` | 频率校准覆盖的 CPU counter owner／epoch |
| `cpu_clock_lower_hz`、`cpu_clock_upper_hz` | 整 epoch／capture 区间正数含端点频率界，lower <= declared CPU rate <= upper，均不超过 `2^32-1` |
| `first_tick`、`last_tick` | extended unsigned 64-bit 的非空含端点窗口 |
| `uncertainty_ticks` | 校准要求的 endpoint 最小 uncertainty bound，单位 reference ticks |

endpoint reference 有 `ticks`、`domain`、`epoch`、`ticks_per_second`、
`uncertainty_ticks`、`uncertainty_known: true`。identity／rate 匹配 admission，完整
误差区间在 capture window 内，不能低报 calibration。误差包括 pairing、sampling
skew、quantization、reference clock accuracy 和该区间 calibration；独立 CPU
frequency envelope 包括 oscillator tolerance、calibration error、条件下 drift。
missing／reversed／foreign-epoch bounds 不准入，nominal HSI／MSIS／PLL 不能给实测
精确频率界。无 live status／epoch 的 MCU timestamp、随意 domain 或 host arrival
timestamp 均不能替代采集事实。

对不倒退 reference delta `d`、总误差 `u`、准入 CPU 频率 `c_min`／`c_max`、reference
rate `r`，按整数计算：`lower = max(0, floor(max(0, d-u)*c_min/r)-1)`、
`upper = ceil((d+u)*c_max/r)+1`。额外 cycle 覆盖 counter sampling phase；reference
bounds 溢出拒绝。upper 必须严格小于 `2^32`，才能接纳
`(end.counter-begin.counter) mod 2^32`，且 residue 须在上述区间。允许数值 wrap，
拒绝歧义 full period、epoch／rate change、counter unavailable 或 unknown error。
已退役 `maximum_interval_cycles` 不作为证据。

schema-1 run／context／trace 保留为原 source／helpers 的历史输入，当前 parser 拒绝；
plan 为 `measurement_schema_version: 2` 且仍未绑定，不改变独立 SoC、OS 或
selected-mode raw protocols。identity file 只证明 metadata 一致，不证明真实 clock
owner、连续性、独立性；真实 station 须采集和审核后才能声称物理测量。

validation 从不变 station／fixture／ELF／resolved／resources 重建 admission；24 小时
以前、future、missing／duplicate metrics、identity／method 不符均拒绝。CLI 不覆盖
inputs、traces、context、workload、instrument 或 admitted tool／config。
成功仅为 `qualified_for_recorded_measurements`：samples 不证明 WCET、未观察负荷、
电气安全或 release。真实 station 执行已审 adapters 并保留 instrument bytes；软件
fixtures 不建立物理结果。host-only 回归经过 commit TDD gate，mock 外部 admission
服务，使用真实 synthetic hashes／parser inputs；admission／resource suite 另链接
真实 ARM ELF，不能凭 synthetic records 认证开发板。

## Workload 与采集配方

`workloads.py plan` 提供三十个显式 station protocols：clock／reset／GPIO／watchdog、
UART／SPI／DMA、I2C、ADC／PWM／EXTI、Flash 掉电、并发负荷、M4F 上下文、
时间／截止时间、全部任务栈／MSP 峰值、生命周期耐久、无节拍定时器端口唤醒，
以及故障停机／外部安全输出。另有 RX DMA／IDLE／连续借用、UART 流控／方向适配器、
连续 ADC、经典 CAN、原始 RMII 以太网、USB FS 设备、捕获和高级 PWM 刹车。
模式检查使用维护的配置特征；选择 UART 不会自动装配 RTS／CTS、DE 或收发器。

`templates/workload.json` 是不完整编写模板，不是已审 station；budgets、instrument
identity、wiring、safety 来自 station owner。缺少 selected controller／mode 保持
`requires_selection`，不支持 CPU／backend 保持 `unsupported`。

typed I/O protocol 还须选择匹配的公共 face。例如 `uart_word_wire` 要求 IRQ 9N1
word port 与保留 bit8 的真实 peer；`spi_dma_whole_cs_segments` 要求同控制器的增强
DMA endpoint。普通 UART 或 polling SPI 不能代替这些模式。目录中的其他 protocol
只提供编写义务；planner 依实际 F407 resolved faces 返回 `selected`、
`requires_selection` 或 `unsupported`，不从名字推定硬件支持。

waveform、drain、instrument 与 null limits 保留到审核。word-only assembly 不能
使用 8-bit smoke adapter；完整 workload／station admission 前须另选受审 8N1
control console。离线 mode selector 不建立该 console 或设备 ready。

```sh
python tools/hil/workloads.py plan \
    --fixture tools/hil/fixtures/stm32f407_qiming_v31.json \
    --resolved build/stm32-qiming-baremetal/generated/resolved.json \
    --report station/workload-plan.json
```

`--workload station/reviewed-workload.json` 检查 strict bindings、stimulation、raw
observations、instrument bytes、safety 和 numeric limits。endurance duration／power-
cut 需正数工作下界，RX bytes、sample／packet／frame／edge counts、flow／break
也需正整数下界，模板全部 null。合法 protocol 仍为 prepared_unbound／not_executed。
recipe、measurement、OS acceptance 都按 admitted fixture 和 canonical resolved config
重验 content-pinned workload；相同 hash 不准入缺刺激、错 controller、无 instrument
bytes 或未审 limits。设备动作前校验编写义务，不能证明仪器描述或物理观察真实。

ThreadX 有独立七项 task、IRQ-wake、context、stack、deadline、lifecycle、fail-stop
观察合同，M4 准备只覆盖配置工具实际维护的 F407 ThreadX 组合，Native 仅 host
软件；不继承 FreeRTOS
低功耗验收。见 [ThreadX acquisition](threadx.md)。计划保持 prepared_unbound／
not_executed，无真实 ThreadX station 结果。其他 peripheral protocols 不会自动提供
CAN peer、USB host、Ethernet link 或 waveform drivers；validator 只准入记录的
smoke／metric／OS scope，不是全部声明 protocol。

fail-stop 要 observer record、incoming masks、retained loans 和 terminal progress。
产品绑定 safe-output port 和真实电气行为；observer callback／terminal kernel
ceiling 不证明 DMA quiescence、安全输出停止或硬件安全资格。

## 显式采集与隔离

`acquire.py plan` 从已有 admission 重建并验证 station-owned recipe，不开设备，
为 `prepared_bound`／`not_executed`。prepare／measure plan／workloads plan 要等
station、wiring、artifact、reviewed limits 齐备才可绑定。

```sh
python tools/hil/acquire.py plan --recipe station/acquisition.json \
    --admission station/admission.json --output station/acquisition-plan.json
```

strict schema-1 `hil_acquisition_recipe` 包含 `admission_sha256`、workload identity、
absolute station-owned `station_lock`，以及精确六步 `identify`、`flash`、
`verify_flash`、`reset`、`serial`、`cleanup`。每步提供 content-pinned executable
`tool`、非空 pinned `inputs`、`arguments`、整数 `timeout_s`（1..3600）和
`maximum_output_bytes`（1..16777216）；probe 步骤绑定准入 tool。审核真实 scripts
和完整 tool／config closure；hash 不沙箱任意 station script。

argv 不经过 shell。整参数 `{record}`、`{session}`、`{admission}`、`{elf}`、`{output}`
分别替换为新 observation path、random public session、不变 admission、准入 ELF、
attempt dir；每 adapter 必须接收前三项。adapter 读取真实 probe／instrument 并写
新 `hil_adapter_observation`，含 `schema_version`、`kind`、`operation`、`session`、
非空 observations。identify 必报真实 part／chip_uid／probe_serial，不符在 flash 前
停止。runner 不猜芯片身份或调试 target。

仅连接且审核通过的站点可执行下列显式设备操作：

```sh
python tools/hil/acquire.py execute --recipe station/acquisition.json \
    --admission station/admission.json --output station/fresh-attempt
```

runner 独占 station lock 并新建 attempt dir，保留精确 stdout／stderr、真实 argv／
exit、timeout／output bounds、observation identity 和不变 admission／recipe。
nonzero exit、missing／stale observation、identity change、output exhaustion 阻止
后续动作，仍尝试 cleanup。cleanup 失败保留 lock 为 quarantine，必须检查设备和
记录，不能自动删 lock 或在旧 evidence 上 retry。每 adapter 前后检查 ownership；
lock 丢失／替换或 operator interrupt 停止全部后续设备动作并保留 quarantine。
capture error／cancel 先 kill／reap process group，保留 raw partial streams。

完整采集成功仅 `captured`／`executed_unqualified`：命令结束不自动通过物理测试。
将真实 smoke／measurement／OS records 分别交 `admit.py`、`measure.py` 和
`os_acceptance.py`。
host subprocess fixture 只测试失败和审计，不操作开发板或建立物理支持。

## 串口 echo adapter

`serial_echo.py` 只打开已准入 `/dev/serial/by-id/...`，跨 open 核对 character device，
使用 `TIOCEXCL` 并恢复原 terminal settings，配置 115200 8N1。admission 要该精确
无流控 byte frame，缺失 legacy metadata 才沿用定义的 8N1 default；word face、7E2
或其他显式 frame 在接设备前拒绝。9N1 须审核 nine-bit peer／waveform，独立 8-bit
诊断 console 与被测 wire 分开；USB 8N1 echo 不认证 nine-bit data／framing。

有界 quiet drain 后发 random public 32-byte challenge，保留 TX／RX／drain 原 bytes
与 host monotonic timestamps；partial read／write 共用有限 capture deadline。
wrong／incomplete echo、output 超限、device replacement、admission 失败保留原失败，
不写成功 observation。

recipe 的 serial step 须 pin Python executable 和 `serial_echo.py` inputs，传
`--admission {admission}`、`--session {session}`、`--record {record}`，placeholder
为独立整参数。审核 `--timeout-s`／`--quiet-s`，默认是 host I/O 上界，不是产品性能
预算。`platform_reference` firmware 才提供 byte echo，UART owner workload 是另一
应用，不能拿来当 echo fixture。

host arrival／close timestamps 不证明 MCU cycles、IRQ latency 或 UART TC。waveform、
clock、Flash power-loss、OS context 需要真实 station instrument 与 reviewed workload。
真实 PTY tests 只验证 POSIX 软件，未操作任何首发板。

## SoC 专项验收

`soc_acceptance.py` 覆盖 [SoC 台账](https://github.com/X-Gen-Lab/nexus/blob/83a27dfbee4e8fbf4d35875b1e9db85af7516eec/docs/design/soc-refinement-execution.csv)
SOC-37–55：十九项受维护 cases 定义 hardware、stimuli、raw observations、
invariants、metric units、safety 与 physical exits。plan 不操作
instrument、不补 numeric limits、不建立 station ready。

```sh
python tools/hil/soc_acceptance.py plan \
    --fixture tools/hil/fixtures/stm32f407_qiming_v31.json \
    --resolved build/stm32-qiming-baremetal/generated/resolved.json \
    --output station/soc-plan.json
python tools/hil/soc_acceptance.py template --case SOC-43 \
    --fixture tools/hil/fixtures/stm32f407_qiming_v31.json \
    --resolved build/stm32-qiming-baremetal/generated/resolved.json \
    --output station/soc-uart-template.json
```

template 的 protocol 复制到 station-owned storage。绑定实际 controller IDs、
instrument identities、reviewed stimuli／observations、isolation／recovery 与全部
metric 的 minimum／maximum／unit。null instruments／limits 故意验证失败；离散
工作量为整数且下界正，不变量失败计数两界均零。scope 服从实际 selected modes，不
包括 case 描述中的所有模式。tickless 须显式选择受维护的 FreeRTOS tickless；
software-reference fixture 仍不能准入物理 station。

正常 admit 和 reviewed workload 后，station serial／instrument operation 以整参数
接绝对 SoC protocol path，recipe inputs 固定 protocol／instrument／calibration。
其他步骤保持 identify／flash／verify_flash／reset／serial／cleanup 顺序。

```sh
python tools/hil/soc_acceptance.py capture \
    --protocol station/soc-uart-protocol.json \
    --recipe station/acquisition.json --admission station/admission.json \
    --output station/fresh-soc-attempt
python tools/hil/soc_acceptance.py validate \
    --admission station/admission.json --raw station/soc-record.json \
    --output station/soc-validation.json
```

capture 复用 exclusive lock、content-pinned no-shell argv、timeout／output bounds、
cleanup／quarantine，生成 `soc-capture.json`；captured 仍是 executed_unqualified，
commit checks 不调用设备。waveform／bus peer／power interruption adapters 由真实站点
提供，工具不制造物理观察。

serial adapter 的 `hil_adapter_observation.observations.soc` 须含 strict schema-1
`soc_hil_observation`：`case_id`、`protocol_sha256`、`context`、`observations`、
`traces`，后三项是本次 operation 真正产生文件的身份；无 captured manifest 的事后
context／trace 拒绝。validation 重放 part／UID／probe 和精确 argv／instrument closure。

schema-1 `soc_hil_context` 含 case_id、session、protocol_sha256、admission_sha256、
实际正数 cpu_clock_hz、load、executor、`mode_scope`、`time_resolution_s`、`methods`。
mode_scope 精确映射 controller IDs 至 `{kind, mode}`；每 metric 的 method 有已审
instrument role、acquisition method、正数 resolution（本 metric 单位）、正数
`window_s`。wall-clock resolution 至多 1 ms，window／duration 不超过真实 serial
acquisition 加该 resolution。IRQ latency 另需 irq_priorities，stack peak 另需
stack_capacity_bytes 且 samples 不超过容量。

schema-1 `soc_hil_trace` 有 case_id、metric、unit、session、protocol_sha256、
context_sha256、canonical `observations_sha256` 和 1–100000 连续
`{sequence, value}`，finite samples 全满足 reviewed limits，不信 pass flag 或 aggregate。

schema-1 `physical_soc_hil` 有 case_id、started_utc、finished_utc、station_id、
board、part、chip_uid、admission_sha256、elf_sha256、resolved_sha256，以及
`protocol`、`capture`、context、observations、traces 文件 identities。observation／
trace maps 精确匹配 catalog／captured serial manifest；时间位于真实 captured interval
且不超过 24 小时。failed、skipped、overlapping、unstarted、quarantined operations，
变化 raw streams、错 session、stale inputs 均拒绝。

SOC-52 另需 `flash_region = {layout, start, size, recovery_policy}`。layout 固定
station／product 编写的 schema-1 `product_flash_test_layout`，含 board、part、
`test_region = {start, size}` 和非空 `protected_regions`（`{id, start, size}`）。
region 匹配外部 layout、精确物理 erase boundaries／density，避开 protected ranges
及全部实际 firmware LOAD；全部 LOAD bytes 都受保护。产品负责 recovery／safe power-
cut，不预留默认分区。

成功为 `qualified_for_recorded_soc_case`，只覆盖 case、选定 modes、准入设备和记录
条件，不证明 WCET、未测负荷、电气安全或产品 release。当前全部物理 SoC cases
仍为 not_executed，synthetic parser／subprocess tests 只证明工具行为。
