# F407 executable-window reference

This public C11 reference exercises an ordinary linked application with explicit
byte reservations inside one physical erase owner. It links the reviewed
STM32F407VET6 startup and system code. It does not execute on hardware or provide
a bootloader, header encoding, trailer encoding, signing or image activation.

The single schema-2 assembly selects `layout.json`. Its application owner is the
complete 128 KiB sector `[0x08020000, 0x08040000)`. The 512-byte prefix and
1,024-byte suffix reserve space without changing erase geometry. The executable
window is `[0x08020200, 0x0803fc00)`, 129,536 bytes. `.isr_vector` begins at that
window's origin; the F407 startup programs VTOR from `g_pfnVectors`. The binary
starts with those vectors and contains no synthesized prefix or suffix bytes.

Consume a complete installed source SDK by its package directory. `SDK_PREFIX`
means the full SDK prefix, including `lib` and `share`. Run from this directory:

```sh
cmake -S . -B build -G Ninja \
  -DNexus_DIR="$SDK_PREFIX/lib/cmake/Nexus" \
  -DCMAKE_TOOLCHAIN_FILE="$SDK_PREFIX/share/nexus/src/cmake/toolchains/arm-gcc.cmake" \
  -DNEXUS_ASSEMBLY_FILE="$PWD/assembly.toml" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --target image_window --parallel 1
```

Byte reproduction also uses the maintained source/build/consumer prefix-map
recipe in `tools/evidence/reproduce.py`. A plain build can retain absolute
`__FILE__` diagnostic strings; moving its SDK can change BIN bytes and size.

Development SDK fixtures additionally require the explicit
`-DNEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON` acknowledgement. No source-checkout fallback
is used. The target's existing ELF resource check validates actual LOADs, vector
words, reset entry, stack reservation and selected image capacity. Products own
the boot path that reaches an offset image and any reserved metadata contents.
The 2,048-byte MSP value is a declared reservation, not a measured peak.

## 中文

本公开 C11 参考只验证普通应用的链接布局。唯一 TOML 选择物理擦除区，再声明
512 字节前缀和 1,024 字节尾部保留量；执行区由它们派生，不另设地址配置入口。
物理区为 `[0x08020000, 0x08040000)`，执行区为
`[0x08020200, 0x0803fc00)`。向量表、ELF LOAD 和 BIN 均从执行区开始；平台
不会生成头部或尾部内容，也不会修改完整 Flash 擦除几何。

上面的命令仅通过安装 SDK 的 `Nexus_DIR` 消费平台。已有链接资源检查会验证
向量表、复位入口、实际加载区与预算。本参考不提供引导器、签名、镜像切换或
实板资格；进入偏移应用的启动路径和保留区内容由外部产品负责。MSP 的 2,048
字节仅为显式保留量，尚未测量实际堆栈峰值。
