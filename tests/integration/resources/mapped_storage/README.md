# 映射存储链接夹具

通过所选 STM32F4 生产目标图链接 `Nexus::MappedStorage`，callback 是本地测试
替身；不实现串行存储控制器、不授权 PCB、不运行器件，不证明 XIP／Cache／DMA
时序。固件排除 `Nexus::NativeMappedStorage`。

ELF 保留 face、共享表、精确 caller state 和两个 map slot。普通 ARM toolchain
配现有 `sky-baremetal.toml`，不保留产品 Flash 区域。
host 行为由 `nexus_mapped_storage_test` 验证。
