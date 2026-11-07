# STM32 芯片集成

当前只维护 [STM32F4 系列包](stm32f4/README.md)，对应锁定的官方 F4 HAL／LL 与 CMSIS
Device F4。其他 STM32 SDK 保留在 `vendors/st/`，不代表仍有生产 provider。

同系列型号共享源码，通过精确料号与受审 profile 在构建期选择差异；不复制每型号
驱动，也不在日常 I/O 按 chip ID 动态选择平台。当前已迁入 F407VET6／F407ZGT6。
体系和证据边界见 [SoC](../README.md) 与[精确支持](../../docs/delivery/support.md)。
