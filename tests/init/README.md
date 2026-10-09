# Init 契约测试

本目录测试显式注册表、固件元数据、统计、非法段边界与重复运行。真实链接回调行为由 `init_registry_tests` 覆盖，故意以乱序声明并注入一个失败条目验证级别排序、继续执行和幂等。

已删除自动 main startup 状态 setter 与合成弱 hook 性能测试；它们不构成启动资格。通用生命周期测试在 `tests/platform/runtime`，平台实际 Reset/向量验证在 firmware ELF checker，实板时序独立 HIL。

实际执行数量由当前 CTest/JUnit 枚举记录，不能沿用历史目录测试计数。CMake 全量测试应启用 `FRAMEWORK_INIT`；可选组件关闭时不创建本目录测试。
