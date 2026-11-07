# SPI endpoint 分配成本

link-only 固件消费公共 factory，验证每个 endpoint 属于其 controller，再用一个
polling loop。`COST_ENDPOINTS=0/1/2/4`；zero 无 SPI controller、face、数组或 ID
引用。非零共用 topology／transfer 结构，一／二保留两个 pointer 数组，四保留四个；
一 endpoint 的未用 slot 保留旧比较形状。

全部选 F407ZGT6、M4F hard float、baremetal、HSE8／PLL168、Os、2048 B MSP、
libc heap 0，相同 Flash／RAM 预算。zero 测共同启动／停止基线，非零包含完整 SPI1、
冷资源和 consumer，不是单独 face 大小。

原 `software-board`、`endpoints-1.toml`／`endpoints-2.toml` 保持原输入：PA5／6／7，
active-low PE4 后加 PE3。四路包另加已审 ZGT6 PG9／PB2，因此除 GPIOA SPI 外还有
E／G／B 三个 CS bank，不声明 PCB、器件、电压、pull 或物理资格。

本 fixture 没有专用根 preset，明确源码与 ARM toolchain：

```sh
cmake -S tests/contracts/io_storage_cost/spi_endpoints -B build/spi-cost-4 \
  -G Ninja -DNEXUS_SOURCE_ROOT=/absolute/nexus/source \
  -DCMAKE_TOOLCHAIN_FILE=/absolute/nexus/source/cmake/toolchains/arm-gcc.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCOST_ENDPOINTS=4
cmake --build build/spi-cost-4 --target endpoint_cost --parallel 1
```

每个 ELF／BIN／map 绑定 source／config、compile command、compiler。普通 gate
测 LOAD／RAM／MSP／heap；对同一 ELF 和配置运行 `interfaces.py`、
`static_storage.inspect_storage`、`retention.py`。非零必须保留 controller 与
endpoint face／table，zero 记完整 face retention 不适用，并验证 SPI face／table／
state 缺失，不能把空报告当通过。storage 子集已计入总 RAM，不再相加。

整镜像与 endpoint／CS state、只读 face、共享表、冷 pin snapshot、应用代码分别报。
2→4 包含 GPIOG／B gate／lease，0→1 包含共享 transport。早期 consumer 只传输
endpoint，未用 controller 被 GC 合法，其 full-retention 曾失败；当前增加 topology
检查并保留 controller。旧一／二 ELF 属于 endpoint-only workload，不是 provider
缺陷或优化回归。链接不测周期、线上时序、动态栈或实物，本夹具不要求 `.su` 参数。
