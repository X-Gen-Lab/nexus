# 外置 provider 生命周期测试夹具

本目录用于完整源码检出的显式 test-support 集成，验证外置包身份、静态连接与 C11
生命周期。它只建模 Native GPIO；普通 Source SDK 和维护消费者计划不携带本夹具。
生产 SoC 使用声明式 package.json 接入，不能照搬这里的测试 Python 插件。

| 文件 | 责任 |
| --- | --- |
| `assembly.toml`、`board/board.json` | 明确包选择与已审 Native PA0／PA3 模型路线 |
| `provider/nexus-soc.json` | 严格身份与 SHA-256 输入 manifest |
| `provider/configure/` | typed 配置、独立状态和共享只读接口 |
| `provider/soc/` | 私有 C adapter、Native GPIO 与有界故障注入 |
| `provider/include/` | 不暴露 vendor／私有状态的测试观察端口 |
| `consumer.c` | 无框架、无 heap 的 C11 验收程序 |
| `lifecycle_test.cpp` | 可选 GoogleTest，调用同一 adapter |

测试 owner 串行调用生命周期。factory 无硬件效果，两个 context 独立而共享只读表。
start 准备两个实例；第二 prepare 注入失败会执行第一个实例真实 C stop，随后两
接口都拒绝 I/O。清故障后可重复 start／stop 恢复。

在复制目录用 CMake、Ninja、C 编译器和 ASM 支持执行：

```sh
cmake -S . -B build -G Ninja \
  -DNEXUS_REFERENCE_PLATFORM_SOURCE=/path/to/nexus -DNEXUS_TEST_SUPPORT=ON
cmake --build build --target external_lifecycle_consumer --parallel 1
./build/bin/external_lifecycle_consumer
```

exit 0 表示模型断言通过，非零由 `consumer.c` 区分查询、正常周期、回滚或恢复失败。
改包前保留构建与日志。默认项目不需要 C++，平台源码路径与测试用途必须显式提供。

检出集成显式传 `-DNEXUS_REFERENCE_PLATFORM_SOURCE=/path/to/nexus`。可选 host
合同设 `-DEXTERNAL_LIFECYCLE_HOST_TESTS=ON` 和
`-DNEXUS_REFERENCE_GOOGLETEST_SOURCE=/path/to/prepared/pinned/googletest`，构建
`external_lifecycle_contract_test` 后生成 fresh Google XML。C++17 仅用于测试，
不会进入 C11 生产程序。

修改源码先评审，再用平台工具更新已有 manifest 输入：

```sh
python /path/to/nexus/tools/configure/integration_package.py \
  refresh-inputs --package provider
python /path/to/nexus/tools/configure/integration_package.py \
  check --package provider --json
```

refresh 更新已声明路径且保留包版本，新增文件需先显式登记；check 不执行包 Python。
测试包只接纳已声明的 inputs（含嵌套私有头／helper），不隐式信任其它文件。
身份验证不代替对受信 Python／CMake 的代码审查。

源码或夹具搬移后使用 fresh build。模型控制头只向两个显式测试目标 PRIVATE 提供；
公共观察端口继续隔离 adapter 状态。Native 只证明模型；F407 ARM 消费者分别证明
编译／链接。新硬件需事实、路由、C 生命周期与独立物理验收，产品任务、
恢复策略、私有应用仍由外部仓库维护。
