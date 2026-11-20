---
id: guides.use-source-sdk
type: how-to
audience: consumer
source_language: zh_CN
---

# 使用 Source SDK 与升级工程

Source SDK 是带选中源码、依赖、许可和 CMake 元数据的可搬移包。应用在自己的
配置下重新编译，不混用其他配置的对象文件。SDK 的制作和平台完整消费者验证由
[维护者发布流程](../development/sdk-publishing.md)负责。

## 选择并验证包

从所选交付记录取得 SDK、校验和及完整源码提交。将整个 prefix 移到使用位置，
设置其路径与期望提交：

```sh
NEXUS_SDK_PREFIX=/absolute/path/to/nexus-sdk
NEXUS_EXPECTED_REVISION=REPLACE_WITH_SELECTED_40_CHARACTER_COMMIT
python "$NEXUS_SDK_PREFIX/share/nexus/src/cmake/package/package_source_sdk.py" \
  --verify "$NEXUS_SDK_PREFIX/share/nexus/src"
```

校验必须成功。缺失、多出、修改或 symlink 的 payload 都会拒绝。正式消费使用
可发布包；开发 fixture 明确是另一范围，不用 opt-in 掩盖身份或接口不匹配。
包已包含所选依赖，不再在 prefix 内执行源码子模块准备或安装工具。

## 配置现有应用

应用保留自己的 CMake、TOML 和 C 源。从应用目录选择独立输出目录：

```sh
cmake -S . -B build/sdk-native -G Ninja \
  -DNexus_DIR="$NEXUS_SDK_PREFIX/lib/cmake/Nexus" \
  -DNEXUS_EXPECTED_SOURCE_REVISION="$NEXUS_EXPECTED_REVISION" \
  -DNEXUS_TEST_SUPPORT=ON
cmake --build build/sdk-native --parallel 2
ctest --test-dir build/sdk-native --output-on-failure --no-tests=error
```

这条命令适用于带 Native 测试的应用，SDK 须包含 `host-support`。ARM 应用使用
另一个构建目录，并在 configure 时增加 prefix 内的
`share/nexus/src/cmake/toolchains/arm-gcc.cmake`；不需要 Native 时省略
`NEXUS_TEST_SUPPORT`。应用的三个 CMake 调用见[CMake 接入](cmake-consumption.md)。

成功出口由应用确定：Native 实际运行与 fresh 测试报告，或 ARM 镜像、向量与资源
报告。配置和链接成功不包含 MCU 实际执行结论。消费后再次运行完整 prefix 校验。

## 升级已有工程

1. 保留应用源与 `.nexus-template.json`，取得下一份 SDK 及精确提交。
2. 阅读对应变更和[源码接口兼容性](../reference/contracts/sdk-format.md)。
3. 在新构建目录切换 `Nexus_DIR` 和期望提交，重新生成并编译全部相关对象。
4. 执行产品需要的 Native、ARM 和硬件检查，再更新产品依赖锁。

模板来源记录描述工程最初从哪里创建，不会自动改成新 SDK 身份。平台版本、factory
接口代际、TOML schema、typed IR 与 SDK manifest schema 各自负责不同合同；相同
版本号不能替代精确提交，也不能证明二进制兼容。

原 Kconfig／宏实例、旧请求字段与目录迁移只按匹配版本的固定记录读取。当前工程用
TOML、强类型 factory 和显式生命周期；不要在未知字段后回退到旧配置或生成文件。
常见失败见[排错指南](troubleshooting.md)。
