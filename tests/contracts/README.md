# 合同与集成夹具

本目录验证公共C接口之外的真实集成边界：生产CMake图、CPU／ABI、静态存储、
startup／vector、provider保留、Source SDK移动，以及明确模式的ARM固件。
每个fixture README列出精确TOML、工具和不建立的资格，不以目录名推断物理支持。

| 类别 | 入口示例 |
| --- | --- |
| CPU／kernel | `cortex_runtime/`、`os_kernel_profile/`、`os_mpu/`、`os_secure_runtime/` |
| 资源成本 | `os_resources/`、`os_wait_cost/`、`io_storage_cost/` |
| 精确SoC装配 | `stm32_assembly/`、`gd32_assembly/`、`g474_assembly/`、`h743_assembly/`、`u575_assembly/` |
| 多控制器与冷态 | `multi_controller/`、`multi_controller_assembly/`、`physical_platform_assembly/` |
| 外部消费 | `test_source_sdk.py`、`external_provider_lifecycle/` |
| 协议互通 | `mqtt_tls_interop/`、`lwip_mqtt_interop/` |
| 启明器件 | `qiming_devices_assembly/`、`qiming_rs485_assembly/` |

生产source graph始终由CMake维护，fixture不建立第二份平台实现。链接工装可能
保留API而不实际发传输，其资源不是最小产品成本。host真实kernel执行、寄存器模型、
ARM链接和HIL分别报告；有界软件参考不应刷写为产品。

使用各子目录维护入口或 `scripts/ci/` runner，输出目录／SDK／工具前提必须满足。
失败保留原日志；修改工具或生产行为按TDD记录RED→GREEN，纯说明不新增镜像测试。
