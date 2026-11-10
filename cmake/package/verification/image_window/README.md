# F407 执行窗口消费参考

本公共 C11 工程链接 STM32F407VET6 的正常 startup／system，在一个物理擦除 owner
内声明字节保留量。它不执行硬件、不提供 bootloader、头尾编码、签名或镜像激活。

唯一 schema-2 装配选择 `layout.json`：完整 128 KiB sector 为
`[0x08020000, 0x08040000)`，前缀 512 B、尾部 1,024 B，派生执行窗口
`[0x08020200, 0x0803fc00)`，共 129,536 B。`.isr_vector` 位于窗口起点，startup
从 `g_pfnVectors` 设置 VTOR；BIN 从向量开始，不合成头尾字节，不改变擦除几何。

在本目录消费完整 SDK，`SDK_PREFIX` 包含 `lib` 和 `share`：

```sh
cmake -S . -B build -G Ninja \
  -DNexus_DIR="$SDK_PREFIX/lib/cmake/Nexus" \
  -DCMAKE_TOOLCHAIN_FILE="$SDK_PREFIX/share/nexus/src/cmake/toolchains/arm-gcc.cmake" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/assembly.toml" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --target image_window --parallel 1
```

普通构建可能保留绝对 `__FILE__`，移动 SDK 可改变 BIN 字节和大小；当前工程只验证
链接布局，不建立跨路径字节复现结论。开发 fixture
还需 `-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON`，没有检出回退。

实际 ELF gate 检查 LOAD、向量词、reset entry、stack reservation 和窗口容量。
产品负责进入偏移应用的启动路径与保留 metadata 内容。2,048 B MSP 只为声明
保留量，不是实测峰值；链接成功不建立实板资格。
