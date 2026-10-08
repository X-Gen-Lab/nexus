# Config 移植指南

Config使用C11公共接口，不直接依赖MCU SDK。产品负责串行管理owner和资源预算，SoC/board通过显式端口接入存储与维护中的密码provider。

1. 预留两块同样大小、可独立擦除的Flash区域，并使linker固件区避开它们。定义精确read/program/erase/sync及1→0和对齐契约。
2. 初始化`nx_storage_t`，调用`config_backend_flash_bind`。缺少真实分区时拒绝持久化能力，不能换成RAM假成功。
3. 依产品SRAM预算缩小`CONFIG_MAX_MAX_KEYS/MAX_VALUE_SIZE/MAX_KEY_LEN`与默认配置；为完整快照提供scratch，保存map和真实任务栈测量。所有value认证开销56字节。
4. Native使用OpenSSL；MCU绑定维护中的密码库或硬件provider，并接真实健康检查的熵源。不支持时返回UNSUPPORTED。维护中的库版本、资源和许可证纳入依赖清单。
5. 密钥存入独立安全vault；重启先注册所有仍被Flash/备份引用的key，再load。轮换前先持久化两代；写报错需重新扫描实际世代。
6. 所有Config调用交给一个管理owner；ISR和控制任务通过队列传递请求。Flash擦写是否暂停取指或违约控制周期必须实测，任务隔离不消除芯片stall。
7. 验证正常重启、所有erase/program/sync断电边界、坏CRC/密文、熵失败、空间耗尽、键删除、namespace同名隔离、轮换恢复以及真实Flash寿命。

F407候选端口是`soc/stm32f407vg/flash.c`，保留sector10/11并把应用FLASH限制为768KiB。GD32尚未有真实Flashprovider；Native文件模型不能当作GD32或STM32硬件验证。

[完整格式、移植合同及软件证据](../../../docs/implementation/storage-security.md)与[受维护密码provider](../../../services/security/README.md)是本轮实现依据。不能照搬旧OSAL mutex假实现或自制AES/随机数。
