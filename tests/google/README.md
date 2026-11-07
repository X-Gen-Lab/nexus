# GoogleTest／GoogleMock 行为合同

测试编译真实Core／Arch／I/O／OS／provider／组件算法，只mock外部硬件、kernel边界
或依赖端口。不要用一份测试内重新实现的状态机证明另一份实现正确。

`.cmake`文件注册维护目标；`*_test.cpp`观察公共行为，`*_model.h`与配套port建立
明确硬件边界。独立实例、拒绝无效果、取消、deadline、QUARANTINED、真实排空与
最后SETTLED访问是主要合同。模型不能证明实物IRQ延迟／DMA吞吐／电气。

先准备锁定 `ext/googletest`，再从仓库根执行普通完整门禁：

```sh
python scripts/ci/tdd_gate.py --all --preset native-debug
```

实际发现的每个Google用例必须无filter／shard执行并产生fresh XML，缺失、跳过、
额外／陈旧报告、失败均拒绝。Debug／Release／sanitizer结果各自记录，不跨模式继承。
C++17与框架只用于host；MCU不链接测试runtime。

TLS peer fixture中的测试凭据及说明有独立来源，不能作为产品信任根；历史证据
保留原source身份。更多工程前提见[测试工具](../../tools/testing/README.md)。
