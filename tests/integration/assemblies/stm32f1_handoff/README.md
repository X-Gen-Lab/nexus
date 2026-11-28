# STM32F103RB 偏移镜像交接软件参考

`Nexus::STM32F103RBHandoff` 是 F103RB 裸机的可选目标，通过 C 链接接口显式选用。
Boot 先完成真实 Platform 停止和 ClockDomain 排空，再将 MSP、Thumb PC 与
目标 ELF 的可执行区间核对。应用向量固定在 `0x08004200`，使用已有偏移装配。

交接保留 PRIMASK，设置 VTOR 和 MSP 并转入应用自己的 Reset_Handler。
参考 App 等待自身 C 初始化完成，在 `main` 中解除屏蔽，再启动 Platform。
运行中的看门狗、RTC、调试、IRQ、DMA 和其他资源均阻止交接。
完整调用契约见 `nexus/soc/stm32f103rb_handoff.h`。

`verify_pair.py` 分别构建两个 ARM 镜像，验证区域、向量、资源和尾部指令。
应用的实际目标文件与最终 ELF 用来确认它没有选入交接模块。验证可从源码
仓库运行，也可从安装并移位的 `stm32f1` Source SDK 运行。

```sh
python tests/integration/assemblies/stm32f1_handoff/verify_pair.py \
    --source-root "$PWD" --output /tmp/f103rb-handoff-check
```

```sh
python "$SDK/share/nexus/src/tests/integration/assemblies/stm32f1_handoff/verify_pair.py" \
    --prefix "$SDK" --output /tmp/f103rb-handoff-sdk-check
```

镜像地址检查与产品选版及认证各有独立责任。此验证记录 ARM 产物与配置，
实板跳转状态为 `not_executed`。
