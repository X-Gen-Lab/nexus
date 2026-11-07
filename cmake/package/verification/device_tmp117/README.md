# 安装 SDK 的 TMP117 消费参考

这两份精确装配由 `cmake/package/verify_consumers.py` 消费安装后的 Source SDK。
公共 C11 组合持有完全结清的软件 I2C endpoint 与时钟，采已知软件值、复制来源，
在独立有限预算下取消并释放绑定。组件是真实实现，endpoint 仅建模寄存器，不证明
硬件转换时序；所选 GPIO 只保留正常生成接口。

Native 执行正向生命周期；F407 只编译、链接、观察资源与 provider 保留。
器件责任见[唯一组件合同](../../../../docs/reference/api/components.md#tmp117-one-shot-sampling-contract)。
这里没有实板接线，也不继承真实 TMP117 资格。
