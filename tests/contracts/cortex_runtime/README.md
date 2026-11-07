# CPU 软件 runtime 链接夹具

一份 runtime TOML 经生产 CMake 入口消费 `Nexus::Runtime`，保留 Core、Arch、所选
OS、实际 kernel exception handler，以及启用的 FP／MVE 探针。

ROM／RAM 地址与 clock 是合成软件参考，没有 SoC startup、reset vector、时钟树、
板级接线或物理资格，不能作为可刷写固件。上下文指令与 API body 只证明代码生成
和依赖闭包。从仓库根运行维护的完整 compiler／link 矩阵：

```sh
python scripts/ci/cortex_runtime.py \
  --compiler /path/to/arm-none-eabi-gcc --output /tmp/cortex-runtime-evidence
```

runner 写每个 CPU facts JSON／runtime TOML，走普通 toolchain，保留命令、配置、
ELF／object hash、未定义符号和指令审计，不建立硬件执行结果。
