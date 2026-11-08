# 厂商 SDK 与来源边界

本目录维护所选固件实现使用的厂商依赖。SoC target 私有选择 Device、startup 和
HAL／LL／SPL；公共产品接口使用 Nexus 类型。一个目录或子模块存在，不表示对应
芯片、全部外设或开发板已接入。

## 当前依赖与许可证

| 来源 | 版本与身份 | 许可证边界 |
|---|---|---|
| `arm/CMSIS_6` | 官方 Arm v6.4.0，gitlink `af1b110e46a1720349f6f125e386e126767699bd` | Apache-2.0，保留 LICENSE 与头部通知 |
| `st/cmsis_device_{f1,f4,g4,h7,u5}` | 五个已登记 STM32 family 的官方 Device 库存，精确 gitlink | 保留上游通知；不建立五个 family 的生产支持 |
| `st/stm32{f1,f4,g4,h7,u5}_hal` | 对应官方 HAL／LL 子模块，精确 gitlink | 保留上游通知 |
| `st/cmsis_device_h5`、`st/stm32h5_hal` | H563 I3C 有界软件参考的锁定 Device／HAL 依赖 | 目录存在不建立完整 H5 SoC／Board 支持 |
| `st/usb_device`、`st/usb_host` | 官方 Device v2.11.4／Host v3.5.3，匹配 STM32CubeF4 v1.28.3；组织 fork 与精确 gitlink | SLA0044 Rev5，保留原始 LICENSE；只供 ST 器件相关集成 |
| `gigadevice/gd32f30x` | 官方 GD32F30x 3.0.3 选定头与 startup，逐文件 source lock | 原始 BSD-3-Clause、Apache-2.0 与 GigaDevice 通知，见该目录 README／LICENSES |
| `gigadevice/gd32f4xx` | 官方 GD32F4xx 3.3.3 Firmware 子树，逐文件 source lock | BSD-3-Clause、Apache-2.0 与两份原始 Arm Cortex-M 分发通知，见该目录 README／LICENSES |

GigaDevice 导入与所选上游字节一致，Git attributes 保留原始行尾。其外层导入 README
也参与来源／notice 身份核对，因此保留原文；本页提供中文导航，不覆盖原始通知。
在仓库根验证选定导入：

```sh
python3 scripts/ci/verify_vendor_source.py --root vendors/gigadevice/gd32f4xx
```

## CPU Core 与 Device 分别选择

保留的 STM32F103、F407、G474、H743、U575 与 GD32F303、F470 来源曾通过共享
`arm/CMSIS_6` 的显式 Core 探针；这不建立七个 family 的当前生产支持。当前生产
仅维护 STM32F4 的精确 F407VE／ZG 实现，其他来源作为库存保留。具体 Device、IRQ、
startup 和 HAL／LL 仍来自各自锁定来源；Core 迁移不自动启用芯片、外设或安全域。
精确版本、上游 pack 字段和许可证身份见 [VERSIONS.md](VERSIONS.md)。

`cmake/platform/CMSISCore.cmake` 拥有内部 `nexus_cmsis_core` target 和
`nexus_use_cmsis_core(target)`。实现目标私有消费 Core include；这与公开的
`Nexus::Core` 通用机制不同，也不是产品配置或普通应用依赖。

原始 GD32F4xx 导入仍保留旧 Core 字节，其 `core_cm4.h` 宏版本为 3.20，文件头另记
V3.30。实际构建选择共享 Core 6.4，不编辑或重新许可旧字节；分发时仍保留原始通知。
使用哪份 Core 由真实编译 include 闭包证明，不能由目录存在推断。

## 源码与实际编译验证

Source SDK 保留 CMSIS-Core 的完整递归 `Include` 和 Apache-2.0 LICENSE，包含
compiler／architecture 头，不复制整个 CMSIS 仓库，也不自动选择 CMSIS-RTOS、
CMSIS-Driver、DSP 或 NN。保留某个头不等于平台已支持其描述的所有 CPU。

在仓库根、已准备锁定 ARM GCC／G++ 的环境中，选择新输出目录执行头探针：

```sh
python -B tools/maintenance/cmsis_core_probe.py --output /tmp/nexus-cmsis-core-probe
```

`--compiler` 与 `--cxx` 可显式指定安装路径。探针对七个 family 编译 16 个 C11／C++17
单元，包括 U575 独立 Secure／NonSecure 上下文；启用 `CMSIS_DISABLE_DEPRECATED=1`，
保存包含 SYSTEM 头的完整 `-MD` 依赖，并拒绝版本宏相同的外来 Core 路径。
`report.json` 记录编译器、对象、依赖和命令收条。缺工具、编译失败、错误闭包均为失败，
不作 skip。这里的 C++17 用于消费头检查，不改变固件 C11 语言边界，也不建立完整链接
或实板资格。回归由既有 testing 域维护，不增加另一套正式计划。

## 组织托管与维护

来源和取回是两个身份：catalogue 记录官方 `upstream_url` 与受审 `fetch_url`，
gitlink 选择精确 commit。注册 Git 依赖从 X-Gen-Lab fork 取回，并以
`refs/tags/nexus/pins/<full-SHA>` 保留所选版本；这不升级 SDK 代码。递归 FreeRTOS、
显式准备与恢复见[依赖托管合同](../docs/design/source-compatibility.md)。

更新需要复核来源、版本、许可证和内容身份，执行匹配的编译／链接及设备合同回归。
Board 接线写入 Nexus adapter 或外部工程，不修改导入文件；配置阶段不联网下载依赖。
厂商代码不因进入本仓库而改用 MIT 许可。普通公共接口不传播厂商 Handle，显式 SDK
bring-up 依赖也不改变该边界；SDK、实际固件和物理验证分别验收。

ST USB 中间件当前只登记源码库存，不进入默认准备、校验、编译或 Source SDK
负载，也不由此提供 Host／HS／class stack。两份 source lock 记录官方提交、tree、
完整文件摘要、许可和既有 HAL／CMSIS／Cube 配对：
[Device](../dependencies/stm32-usb-device.lock.json)、
[Host](../dependencies/stm32-usb-host.lock.json)。后续适配保留一个控制器状态 owner、
调用方静态存储和完整排空，不使用厂商模板中的隐式 heap 或全局单实例池。
