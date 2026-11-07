# U575 LPTIM1 单组有限 DMA 接收器

`Nexus::U575LpdmaLptim1` 是精确 STM32U575ZIT6Q、TZEN0、baremetal 可选参考；
链接 `Nexus::U575Stop2Lptim1` 选择同一 concrete Stop2 owner。WUT、SRAM4-copy
和 LPTIM1 alternatives 拒绝共存，默认不选择任何一个。固件为静态无分配 C11，
vendor types 保持私有。

## 输入、装配与启动

API 只接收 SRAM4 内一个 `{CCR1, RCR=0, ARR}` tuple，要求 `CCR1<ARR`、
`1023<=ARR<=65535`，另有独立对齐 48 字节 SRAM4 workspace。不开放任意 peripheral
address、调用者 DMA instruction 或 repeated tuple；第二组 tuple 需要单独证明
bus delay 下 preceding-write-OK，当前没有此能力。

先以该 Stop2 identity 初始化 ordinary retained time，再初始化 owner，最后显式
receiver start。start 取得 cold LPTIM1 access、LSE kernel selection 和仅需要的
LSE-system gate，保留 RTC／LSE 既有 provenance。部分 start 效果保留到 public stop
成功，应用不需私有 RCC 写入。已选 SRAM4／LPDMA1 startup 与 retained RTC boot
合同继续适用。

## 深等待与更新顺序

关闭并排空精确 RUN barrier，stop Platform 后，从既有 privileged masked MSP
Thread boundary 进入。RUN LPTIM2 已 disabled 并 joined，不会复用为 Stop2 producer。
专用 owner 在精确 profile 校验后才 enable LPTIM1；post-ENABLE retained-clock
采样与有界 source progress 证明所需 counter-cycle delay。初始 register writes
须有新鲜 write-OK flags，随后才 arm DMA program。

LPTIM1 update request13／BREQ1 门控 CCR1 word；两个生成的软件 continuation LLIs
依次写不连续 RCR，最后 ARR，ARR 清 UE。WUT 是独立逃逸源；不用 AlarmA trigger 或
GPIO route。request 成功表示十二个 APB bytes 及各自新鲜 kernel write-OK 完成，
不证明输出播放或实际在 Stop2 驻留。

## 借用、撤回与恢复

ACCEPTED 借用 request、tuple 和完整 workspace 到 acquire-observed SETTLED；
cancel／deadline 不撤销存储。先撤 producer UEDE／DIEROK，再 DMA suspend／reset／
idle 和引用删除；自有 RCC reset 随后按 ES0499 关闭 LPTIM1，绝不清 CR.ENABLE。
全部 peripheral、IRQ、loan gate 和 Stop2 clock restoration 完成后才最终 release。
public start 的 access 效果保留到已 joined 的 public stop。

未知 access profile 保留效果并禁止 peripheral access；恢复重试同一链，不再次 WFI。
观察外部 DMA／LPTIM reset 会永久 quarantine 活跃 loan，reset defaults 不能证明
最终访问结束。自有 reset withdrawal failure 在 DMA proof 后仍可重试。

## 源码与资格边界

公共头 `include/nexus/components/stm32u575_lpdma_lptim1.h` 定义具体合同；详见
[设计说明](../../../../docs/design/u575-lptim1-lpbam.md) 与
[FIG-28 精确参考图](../../../../docs/assets/diagrams/u575-lptim1-lpbam.svg)。
这是 IO-32／SOC-35 的有界软件参考，完整退出仍开放。Board routing 未知，全部
物理 timing、waveform、retention、wake 和 power HIL 为 `not_executed`。
