# Shell 验证入口

以 CMake/CTest 注册的生产行为检查为准，不从测试定义推断执行通过。保留 nonzero executed count、JUnit、effective config 和真实 target/toolchain 身份。模型 port 不计实板资格。当前记录见 [组件重构](../../../docs/implementation/components-refactor.md)。
