---
id: status.ledgers.io-evolution
type: status
audience: maintainer
source_language: zh_CN
---

(status-ledgers-io-evolution)=
# I/O 增量实施台账

本页由原 CSV 派生，不是新的状态权威、执行记录或软件资格。
原编号、顺序和全部原字段来自 [原台账](../../design/io-evolution-execution.csv)。
源文件：`docs/design/io-evolution-execution.csv`；SHA-256：`1c6b255dba79e239d269bc5a86dbe6e07ce10c90fad06e875fa9b952255fd24d`。

不同台账范围重叠，不相加数量或计算全项目完成率。实现状态与软件、
物理状态分别解释；`development_passed` 不表示当前文档 HEAD 已执行。
`qualification_record_authority` 要回到精确结果索引判断源码和范围。
绝对 `/workspace`、`/dev/shm` 定位保留原字节，仅说明原执行主机位置；
可取得归档及 member 定位见 [软件证据入口](../software-verification.md)。

**原退出字段**只有原 CSV 中实际存在的字段；任务、资格前提和合同链接
也按原文保留，不能据此补造某次执行。

本表包含 42 条原记录，仅表示来源行数。

```{contents}
:local:
:depth: 1
```

(status-io-evolution-io-01)=
## IO-01

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-01 |
| `phase` | P0 |
| `area` | capabilities |
| `task` | Single authority for exact provider and mode support |
| `implementation_paths` | tools/configure/selected_contracts.py;tools/configure/io_traits.py;tools/configure/factory.py;tools/maintenance/work_packages.py;docs/delivery/capabilities.md |
| `exit_condition` | Existing catalogue plus selected metadata; document exact boundaries |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-02)=
## IO-02

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-02 |
| `phase` | P0 |
| `area` | traits |
| `task` | Optional immutable typed I/O traits |
| `implementation_paths` | tools/configure/io_traits.py;tools/configure/factory.py |
| `exit_condition` | Capability switch controls all emitted readonly trait objects |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-03)=
## IO-03

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-03 |
| `phase` | P0 |
| `area` | silicon |
| `task` | Observed silicon identity and revision admission |
| `implementation_paths` | core/include/nexus/core/silicon.h;core/src/silicon.c |
| `exit_condition` | Exact documents and revision policy; unknown is explicit; factory stays cold |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-04)=
## IO-04

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-04 |
| `phase` | P0 |
| `area` | lifecycle |
| `task` | Admission cancellation drain epochs and quarantine regression matrix |
| `implementation_paths` | core/src/request.c;tests/google/io_state_machine_test.cpp;tests/google/spi_dma_owner_wait_test.cpp |
| `exit_condition` | Fresh complete no-skip software gate for this iteration |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-05)=
## IO-05

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-05 |
| `phase` | P0 |
| `area` | budgets |
| `task` | Bounded IRQ and service execution observation |
| `implementation_paths` | io/include/nexus/io/cycle_probe.h;io/src/cycle_probe.c;tools/configure/factory.py |
| `exit_condition` | Optional probes compile out; explicit service and IRQ budgets |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-06)=
## IO-06

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-06 |
| `phase` | P0 |
| `area` | stream |
| `task` | Rate block hold-time loss and backpressure budgets |
| `implementation_paths` | tools/configure/stream_profiles.py;io/include/nexus/io/stream_profile.h;io/src/stream_profile.c |
| `exit_condition` | Actual selected-provider configuration validates stream budget |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-07)=
## IO-07

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-07 |
| `phase` | P0 |
| `area` | hil |
| `task` | Board fixtures and fault-injection station preparation |
| `implementation_paths` | tools/hil;tools/testing/test_hil_io_protocols.py |
| `exit_condition` | Prepare software tooling; physical tests remain not_executed |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-08)=
## IO-08

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-08 |
| `phase` | P0 |
| `area` | measurement |
| `task` | Linked resource and runtime cost comparison |
| `implementation_paths` | tools/measurement;tests/contracts/uart_rx_compact_assembly |
| `exit_condition` | Fresh exact ELF comparison; no inferred physical timing or energy |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-09)=
## IO-09

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-09 |
| `phase` | P1 |
| `area` | resources |
| `task` | Hierarchical controller base channel resource claims |
| `implementation_paths` | tools/configure/resource_graph.py;tools/configure/providers/stm32g474/pwm_shared.py;soc/stm32g474/drivers/pwm_shared.c;tests/google/g474_pwm_shared_factory_test.cpp;tools/measurement/retention.py |
| `exit_condition` | Compatible shared base plus channels requires one actual hardware owner |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-10)=
## IO-10

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-10 |
| `phase` | P1 |
| `area` | resources |
| `task` | Physical peripheral identity and mutually exclusive personalities |
| `implementation_paths` | tools/configure/resource_graph.py;tools/configure/matrix.py;soc/stm32g474/soc.json |
| `exit_condition` | Canonical aliases and personality conflicts rejected |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-11)=
## IO-11

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-11 |
| `phase` | P1 |
| `area` | events |
| `task` | Typed hardware event graph and startup ordering |
| `implementation_paths` | tools/configure/trigger_graph.py |
| `exit_condition` | Validate event cycles with owned stop gates; emit selected reviewed routes |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-12)=
## IO-12

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-12 |
| `phase` | P1 |
| `area` | dma |
| `task` | Selected DMA engines lanes requests and reachability |
| `implementation_paths` | tools/configure/dma_topology.py |
| `exit_condition` | Exact provider tuples; reject physical lane collisions |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-13)=
## IO-13

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-13 |
| `phase` | P1 |
| `area` | dma |
| `task` | Descriptor ownership placement cache and chain cancellation |
| `implementation_paths` | tools/configure/dma_topology.py;soc/stm32u575/drivers/uart_dma_chain.c;soc/stm32u575/private/stm32u575_uart_gpdma_chain.h;soc/stm32u575/private/stm32u575_uart_gpdma_impl.h;tests/google/u575_uart_chain_test.cpp;tools/configure/assemblies/u575-uart-chain.toml |
| `exit_condition` | Static validation plus actual linked-list provider drain and loans |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-14)=
## IO-14

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-14 |
| `phase` | P1 |
| `area` | uart |
| `task` | Explicit line formats flow control and independent nine-bit I/O |
| `implementation_paths` | io/include/nexus/io/uart_word.h |
| `exit_condition` | Native and exact STM32/GD32 register paths; compact byte default |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-15)=
## IO-15

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-15 |
| `phase` | P1 |
| `area` | uart |
| `task` | Bounded FIFO IRQ service and concurrent stream load |
| `implementation_paths` | io/private/uart_fifo_impl.h;soc/stm32h743/drivers/uart_fifo.c;soc/stm32u575/drivers/uart_fifo.c;docs/design/uart-fifo.md |
| `exit_condition` | Bounded batch and IRQ latency observations on exact providers |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-16)=
## IO-16

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-16 |
| `phase` | P1 |
| `area` | spi |
| `task` | Whole-CS segments dummy cycles and 8/16-bit words |
| `implementation_paths` | io/include/nexus/io/spi_dma_owner_binding.h;io/native/spi_dma_transaction.c;components/bus_owner/src/spi_dma_owner.c;soc/stm32f407/drivers/spi_dma.c;soc/gd32f470/drivers/spi_dma.c |
| `exit_condition` | Native STM32F407 GD32F470 polling then async DMA loan and drain |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-17)=
## IO-17

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-17 |
| `phase` | P1 |
| `area` | i2c |
| `task` | Kernel-clock timing and long-message reload |
| `implementation_paths` | soc/common/i2c_v2.c;tools/configure/i2c_timing.py;tools/configure/i2c_timing_selection.py;tests/google/i2c_v2_long_factory_test.cpp |
| `exit_condition` | Reviewed electrical timing inputs; preserve message START and STOP |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-18)=
## IO-18

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-18 |
| `phase` | P1 |
| `area` | adc |
| `task` | Explicit sample formats groups triggers and provenance |
| `implementation_paths` | io/include/nexus/io/adc_format.h;io/include/nexus/io/adc_group.h;io/src/adc_group.c;soc/stm32g474/drivers/adc_group.c;docs/design/adc-group.md |
| `exit_condition` | Integer storage metadata and actual provider group support |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-19)=
## IO-19

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-19 |
| `phase` | P1 |
| `area` | pwm |
| `task` | Atomic multi-channel shadow commit and break fault handling |
| `implementation_paths` | io/include/nexus/io/pwm_group.h;soc/stm32g474/drivers/pwm_group.c;tools/configure/providers/stm32g474/emission.py |
| `exit_condition` | Single timer owner; no automatic restart after break |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-20)=
## IO-20

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-20 |
| `phase` | P1 |
| `area` | capture |
| `task` | Counter widths filters encoders and overcapture |
| `implementation_paths` | io/include/nexus/io/timer_capture.h;io/include/nexus/io/encoder.h;soc/stm32g474/drivers/timer_capture.c;soc/stm32g474/drivers/encoder.c |
| `exit_condition` | Wrap-safe capture and reviewed encoder modes on real providers |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-21)=
## IO-21

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-21 |
| `phase` | P1 |
| `area` | timestamps |
| `task` | Timestamp source domain epoch frequency and uncertainty |
| `implementation_paths` | io/include/nexus/io/observation.h;io/src/observation.c;io/src/eth_frames_mac.c;tests/google/ethernet_frames_mac_test.cpp;docs/design/ethernet-frames.md |
| `exit_condition` | Reject incompatible domains and distinguish observation from event time |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-22)=
## IO-22

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-22 |
| `phase` | P1 |
| `area` | diagnostics |
| `task` | Optional state recovery ownership and checked service entry |
| `implementation_paths` | io/src/observation.c |
| `exit_condition` | Checked dispatch is distinct from I/O progress or settlement |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-23)=
## IO-23

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-23 |
| `phase` | P1 |
| `area` | memory |
| `task` | Remove unused TX DMA fields from RX-only providers |
| `implementation_paths` | soc/stm32f407/drivers/uart_receive.c;soc/stm32f407/drivers/uart_duplex.c;soc/gd32f470/drivers/uart_receive.c;soc/gd32f470/drivers/uart_duplex.c;tests/contracts/uart_rx_compact_assembly |
| `exit_condition` | Fresh linked RAM measurements and duplex lifecycle regression |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-24)=
## IO-24

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-24 |
| `phase` | P1 |
| `area` | flash |
| `task` | Busy-bank pulse RAM closure cache ECC and watchdog contract |
| `implementation_paths` | soc/stm32h743/drivers/flash.c;tools/configure/providers/stm32h743/emission.py;soc/stm32h743/system.c;tools/measurement/h743_flash_ram.py;tools/measurement/flash_execution.py;tools/testing/test_h743_ram_build_gate.py;docs/design/h743-flash-ram.md |
| `exit_condition` | Uncancellable pulse and complete execution closure verified per profile |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-25)=
## IO-25

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-25 |
| `phase` | P1 |
| `area` | providers |
| `task` | Reuse reviewed peripheral IP generations and official HAL/LL |
| `implementation_paths` | soc/common;io/private/uart_fifo_impl.h;io/src/eth_frames_mac.c;tools/configure/selected_contracts.py |
| `exit_condition` | Private implementation sharing with exact per-family facts |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-26)=
## IO-26

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-26 |
| `phase` | P1 |
| `area` | h743 |
| `task` | Cold PLL voltage latency and kernel-clock profiles |
| `implementation_paths` | soc/stm32h743/clock.c;soc/stm32h743/system.c;tools/configure/clocks.py;tests/google/h743_high_factory_test.cpp;docs/design/h743-high-clock.md |
| `exit_condition` | Readback rollback and exact reference identity |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-27)=
## IO-27

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-27 |
| `phase` | P1 |
| `area` | h743 |
| `task` | UART continuous duplex DMA and SPI DMA with cache |
| `implementation_paths` | soc/stm32h743;soc/stm32h743/drivers/uart_continuous.c;soc/stm32h743/private/stm32h743_uart_continuous.h;soc/stm32h743/drivers/uart_duplex.c;soc/stm32h743/private/stm32h743_uart_duplex.h;soc/stm32h743/drivers/spi_dma.c;tests/google/h743_uart_continuous_test.cpp;tests/google/h743_uart_duplex_test.cpp;tests/google/h743_spi_dma_test.cpp;tools/configure/assemblies/h743-uart-continuous.toml;tools/configure/assemblies/h743-uart-continuous-cache-on.toml;tools/configure/assemblies/h743-uart-duplex-continuous.toml;tools/configure/assemblies/h743-uart-duplex-continuous-cache-on.toml;tools/configure/assemblies/h743-spi-dma-cache-on.toml |
| `exit_condition` | Selected SRAM reachability and held-buffer protection |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-28)=
## IO-28

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-28 |
| `phase` | P1 |
| `area` | u575 |
| `task` | Finite GPDMA provider and USART errata handling |
| `implementation_paths` | soc/stm32u575/drivers/uart_dma.c;soc/stm32u575/private/stm32u575_uart_gpdma.h;tests/google/u575_uart_gpdma_test.cpp;tests/google/u575_uart_gpdma_factory_test.cpp;docs/design/u575-uart-gpdma.md |
| `exit_condition` | Actual GPDMA drain and exact ES0499 workarounds; TZEN0 first |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-29)=
## IO-29

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-29 |
| `phase` | P1 |
| `area` | g474 |
| `task` | Synchronous PWM ADC trigger and comparator break reference |
| `implementation_paths` | io/include/nexus/io/pwm_adc.h;soc/stm32g474/drivers/pwm_adc.c;tools/configure/providers/stm32g474/coupled.py;tests/google/g474_pwm_adc_factory_test.cpp;docs/design/pwm-adc.md |
| `exit_condition` | Generic control capture mechanisms; product algorithms external |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-30)=
## IO-30

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-30 |
| `phase` | P2 |
| `area` | power |
| `task` | Optional runtime clock and power transitions |
| `implementation_paths` | tools/configure/runtime_power.py;soc/stm32h743/system.c;docs/design/clock-domain-sleep.md |
| `exit_condition` | Active I/O leases rollback and monotonic deadline continuity |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-31)=
## IO-31

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-31 |
| `phase` | P2 |
| `area` | pinctrl |
| `task` | Explicit default sleep safe levels wake and retention |
| `implementation_paths` | io/include/nexus/io/pin_state.h;soc/stm32g474/drivers/pin_state.c;tools/configure/pin_states.py;docs/explanation/gpio-pin-states.md |
| `exit_condition` | No invented board wiring; selected reviewed state only |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-32)=
## IO-32

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-32 |
| `phase` | P2 |
| `area` | u575 |
| `task` | LPDMA autonomous Stop2 and LPBAM reference |
| `implementation_paths` | soc/stm32u575;components/stm32u575_lpdma_copy;components/stm32u575_stop2/include/nexus/components/stm32u575_stop2_copy.h;soc/stm32u575/drivers/lpdma_copy.c;soc/stm32u575/private/stm32u575_lpdma_copy_hw.h;soc/stm32u575/memory_init.c;soc/stm32u575/system.c;tests/google/u575_lpdma_copy;tests/google/u575_stop2_copy;tests/google/u575_memory_test.cpp;tools/configure/assemblies/u575-lpdma-copy-stop2.toml;cmake/package/verification/io_evolution/consumers/u575-lpdma-copy.c |
| `exit_condition` | Selected low-power peripheral SRAM4 and wake/stop ownership |
| `implementation_status` | partial_exact_reference_implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-33)=
## IO-33

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-33 |
| `phase` | P2 |
| `area` | canfd |
| `task` | CAN FD payload timing BRS ESI and message RAM |
| `implementation_paths` | io/include/nexus/io/can_fd.h;soc/stm32g474/drivers/fdcan.c;tools/configure/providers/stm32g474/coupled.py;tests/google/g474_fdcan_test.cpp;tests/google/g474_fdcan_factory_test.cpp;docs/design/can-fd.md |
| `exit_condition` | Classic CAN remains compact; exact FDCAN provider required |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-34)=
## IO-34

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-34 |
| `phase` | P2 |
| `area` | ethernet |
| `task` | VLAN scatter gather frame loans and hardware timestamps |
| `implementation_paths` | io/include/nexus/io/ethernet_frames.h;io/src/eth_frames_mac.c;soc/stm32f407/drivers/ethernet_frames.c;soc/gd32f470/drivers/ethernet_frames.c;docs/design/ethernet-frames.md |
| `exit_condition` | Actual hardware descriptor ownership and truthful global abort |
| `implementation_status` | implemented |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-35)=
## IO-35

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-35 |
| `phase` | P2 |
| `area` | usb |
| `task` | Optional class adapters and exact PHY endpoint capabilities |
| `implementation_paths` | io/src/usb_device_controller.c;io/native/usb_device.c;components |
| `exit_condition` | Control reset ACK ZLP and device mode before additional USB modes |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-36)=
## IO-36

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-36 |
| `phase` | P2 |
| `area` | time |
| `task` | RTC LPTIM alarms wrap backup and reset domains |
| `implementation_paths` | io/include/nexus/io/rtc.h;io/include/nexus/io/lptim.h;io/native/rtc_lptim.c;soc/stm32u575/drivers/lptim.c;soc/stm32u575/private/stm32u575_lptim.h;tools/configure/assemblies/u575-lptim-run.toml;tests/google/u575_lptim_test.cpp;soc/stm32u575/drivers/rtc.c;soc/stm32u575/private/stm32u575_rtc.h;tools/configure/assemblies/u575-rtc-run.toml;tests/google/u575_rtc_test.cpp;docs/design/io-extension-boundaries.md |
| `exit_condition` | Wall-clock and monotonic domains remain separate |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-37)=
## IO-37

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-37 |
| `phase` | P2 |
| `area` | storage |
| `task` | SDMMC and serial memory block command mapped interfaces |
| `implementation_paths` | io;soc;components/mapped_storage;tests/google/mapped_storage_test.cpp;docs/design/io-extension-boundaries.md |
| `exit_condition` | XIP cache DMA loans and erase ownership are explicit |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-38)=
## IO-38

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-38 |
| `phase` | P2 |
| `area` | i3c |
| `task` | Bounded discovery IBI and hot-join caller storage |
| `implementation_paths` | components/i3c_controller;tests/google/i3c_controller_test.cpp;tests/contracts/i3c_runtime;docs/design/i3c-controller.md |
| `exit_condition` | No implicit factory discovery; exact peripheral reference required |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-39)=
## IO-39

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-39 |
| `phase` | P2 |
| `area` | security |
| `task` | TrustZone peripheral broker and attribution |
| `implementation_paths` | components/user_io_broker;components/bus_owner;tests/google/user_io_async_test.cpp;docs/design/user-io-broker.md;tests/contracts/u575_trustzone |
| `exit_condition` | Principal and memory validation; copied and borrowed data distinct |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-40)=
## IO-40

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-40 |
| `phase` | P2 |
| `area` | multicore |
| `task` | AMP resource partition and epoch-safe IPC |
| `implementation_paths` | core;arch;components/amp_ipc;tests/google/amp_ipc_test.cpp;docs/design/io-extension-boundaries.md |
| `exit_condition` | Unique hardware owner and explicit remote reset/cache contract |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-41)=
## IO-41

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-41 |
| `phase` | P2 |
| `area` | audio |
| `task` | I2S SAI PDM framing and continuous DMA loans |
| `implementation_paths` | io;soc;io/include/nexus/io/frame_stream.h;io/src/frame_stream.c;tests/google/frame_stream_test.cpp;docs/design/io-extension-boundaries.md;components/f407_i2s2_slave_tx;soc/stm32f407/drivers/i2s2_slave_tx.c;soc/stm32f407/private/stm32f407_system.h;tests/google/f407_i2s2_slave_tx_test.cpp;tests/contracts/f407_i2s2_slave_tx;tools/configure/assemblies/sky-i2s2-slave-tx.toml;tools/configure/assemblies/qiming-i2s2-slave-tx.toml;docs/design/f407-i2s2-slave-tx.md;tools/testing/evidence/f407-i2s2-20261020/README.md |
| `exit_condition` | Frame-slot format and bounded underrun; optional external codec |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |

(status-io-evolution-io-42)=
## IO-42

| 原字段 | 原值 |
| --- | --- |
| `task_id` | IO-42 |
| `phase` | P2 |
| `area` | multimedia |
| `task` | Camera display accelerator planar and strided buffers |
| `implementation_paths` | io;soc;io/include/nexus/io/frame_stream.h;components/frame_dma2d;tests/google/h743_frame_dma2d_test.cpp;docs/design/io-extension-boundaries.md |
| `exit_condition` | Frame completion backpressure external memory and 2D DMA ownership |
| `implementation_status` | in_progress |
| `software_status` | not_qualified |
| `hardware_status` | not_executed |
| `maintenance_owner` | Codex |
