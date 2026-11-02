# Installed TMP117 reference

These two exact assemblies are consumed through the installed Source SDK by
`cmake/package/verify_consumers.py`. Their shared C11 composition owns a settled
software I2C endpoint and explicit clock. It samples a known software value,
copies provenance, cancels under a separate finite budget and drops the binding.
The component is real; the endpoint models registers and establishes no hardware
conversion timing. The selected GPIO only retains an ordinary generated face.

Native executes the positive lifecycle. F407 only compiles, links and inspects
actual resources/provider retention. See the [canonical component contract](../../../../docs/reference/api/components.md#tmp117-one-shot-sampling-contract)
for physical scope and caller responsibilities. No physical wiring is supplied.

## 中文

这两份精确装配通过 `cmake/package/verify_consumers.py` 消费已安装 Source SDK。
公共 C11 组合显式持有完全结清的软件 I2C endpoint 与时钟，读取已知软件数值、
复制来源信息，使用独立有限预算取消后放弃绑定。组件实现真实运行，endpoint 仅
建模寄存器，不建立硬件转换时序结论；所选 GPIO 只保留普通的生成接口。

Native 实际执行正向生命周期，F407 只编译、链接并检查实际资源与 provider 保留。
器件范围与调用者责任见[唯一组件合同](../../../../docs/reference/api/components.md#tmp117-one-shot-sampling-contract)。
这里不提供实板接线。
