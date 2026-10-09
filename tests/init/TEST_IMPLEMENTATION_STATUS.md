# Init 测试状态

保留注册表与固件元数据的实际 API/属性测试，新增精简显式集成检查。真实 linker registry 的顺序、失败统计和幂等回归独立运行。

删除旧自动 main wrapper 相关测试状态 setter、弱 hook 空调用与合成内存/性能估算。测试数量减少是移除无生产合同覆盖的旧断言，不以数量证明实现质量。

以当前 source/config/toolchain 下实际 CTest 执行报告为验收依据；尚未执行的 MCU/实板路径不能由 Native 结果继承。
