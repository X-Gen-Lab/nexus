移植指南
========

本指南说明如何将 Nexus 移植到新的 MCU 平台。

步骤
----

1. 创建平台目录：``platforms/<platform_name>/``
2. 实现 HAL 驱动
3. 创建启动代码和链接脚本
4. 添加 CMake 配置
5. 使用 blinky 示例测试

参考实现请查看 ``platforms/stm32f4/``。
