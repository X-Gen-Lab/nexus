# External provider lifecycle reference

This software-only package exercises the external family contract. Its two GPIO
views use reviewed Native PA0/PA3 model routes, independent static contexts and
one readonly operation table. The package-owned C adapter reuses Native GPIO
operations and exposes a bounded, single-owner test fault port. It is not a
product, new chip driver or physical hardware claim.

The consumer checks pure factory lookup, instance isolation, second-prepare
failure, actual first-instance C rollback, withdrawn I/O and repeated recovery.
It does not access private Nexus types. The adapter's private include scope may
use the reviewed Native provider interface.

Use the installed SDK explicitly; the default project requires only C11 and ASM:

```sh
cmake -S . -B build -G Ninja -DNexus_DIR=/path/to/sdk/lib/cmake/Nexus
cmake --build build --target external_lifecycle_consumer --parallel 1
./build/bin/external_lifecycle_consumer
```

Host GoogleTest contracts are optional. Enable `EXTERNAL_LIFECYCLE_HOST_TESTS`
and pass `NEXUS_REFERENCE_GOOGLETEST_SOURCE` for the separately prepared, pinned
framework. They require C++17; neither that framework nor its runtime is linked
into the C11 consumer. Repository tests explicitly pass
`NEXUS_REFERENCE_PLATFORM_SOURCE` instead of using an installed SDK; there is no
automatic checkout fallback.

# 外置 provider 生命周期参考

此包只验证软件接入合同。两个 GPIO 使用已审 Native PA0／PA3 模型路由、独立静态
上下文及共享只读操作表。包内 C 适配器复用 Native GPIO 算法，提供有界、单一所有者
测试故障端口；它不是产品、新芯片驱动或实板资格。

消费者实际检查纯 factory 查询、实例隔离、第二实例 prepare 失败、第一实例 C stop
回滚、I/O 撤回及重复恢复。产品源码只使用公共 Nexus 接口和小型测试端口头文件；
已审 Native 私有接口仅在适配器自身的私有编译范围内可见。

默认构建只需要 C11、ASM 和显式 SDK 前缀。可选 GoogleTest 合同需要另行准备的
固定版本框架及 C++17，不进入 C11 消费者。仓库测试会显式选择源码树入口，没有
找不到 SDK 后自动回退到 checkout 的行为。修改包内容后必须重新审核并更新
`nexus-soc.json` 输入摘要；测试结果不建立硬件、安全或发布资格。
