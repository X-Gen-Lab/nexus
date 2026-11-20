---
id: tutorials.first-native
type: tutorial
audience: consumer
source_language: zh_CN
---

# 运行第一个 Native 应用

本教程在 Linux 主机上运行一个 C11 GPIO／UART 应用，观察一次传输结清和平台停止。
Native 是软件模型，需要显式启用测试支撑；本教程不运行平台完整维护测试图。

## 准备输入

需要 Git、Python 3.11+、CMake 3.31+、Ninja 和主机 C 编译器。按
[获取源码](../guides/source-checkout.md)取得平台与公开示例仓库，记录所选提交。
设置路径和示例仓库的完整提交；`PRODUCT_DIR` 必须尚不存在：

```sh
NEXUS_SOURCE=/absolute/path/to/nexus
NEXUS_EXAMPLES=/absolute/path/to/nexus-examples
NEXUS_EXAMPLES_REVISION=REPLACE_WITH_SELECTED_40_CHARACTER_COMMIT
PRODUCT_DIR=/tmp/my-nexus-native
```

示例模板的提交与平台提交各自记录。创建器检查模板清单及接口代际，不根据相同版本号
猜测它们是同一份源码。

## 创建与准备

从任意工作目录执行：

```sh
python "$NEXUS_SOURCE/tools/dev/dev.py" create \
  --templates "$NEXUS_EXAMPLES" --revision "$NEXUS_EXAMPLES_REVISION" \
  --template native --source "$NEXUS_SOURCE" --test-support \
  --output "$PRODUCT_DIR"
python "$NEXUS_SOURCE/tools/dev/dev.py" prepare --scope assembly \
  --assembly "$PRODUCT_DIR/assembly.toml" --test-support
```

创建器复制维护的 CMake、TOML、C 源和编辑器配置，解析所选装配，并保存
`.nexus-template.json`。输出目录已存在、模板变更或配置拒绝时，先处理诊断。
最小 Native 应用没有外部 Git 运行依赖；需要网络、TLS 等组件时，准备器按 TOML
选择供应方，显式加 `--allow-network` 才允许获取它们。

## 编译与运行

```sh
cmake -S "$PRODUCT_DIR" -B "$PRODUCT_DIR/build" -G Ninja \
  -DNexus_DIR="$NEXUS_SOURCE/cmake" -DNEXUS_TEST_SUPPORT=ON
cmake --build "$PRODUCT_DIR/build" --parallel 2
ctest --test-dir "$PRODUCT_DIR/build" --output-on-failure --no-tests=error
```

成功时 `starter_application` 通过，直接运行生成的程序也会输出：

```text
Nexus starter: transfer settled; platform stopped
```

这条路径执行真实应用 C，UART 由模型提供。模板使用静态请求和 payload，只有请求
真正结清后才停止平台。Native UART 不连接主机串口。

打开 `main.c`、`assembly.toml` 和 `.nexus-template.json`，分别查看执行、选择与来源。
修改应用 C 后重跑 build；修改 TOML 后先检查配置，再重跑 build。

## 下一步

给现有应用增加接口见[应用配置](../guides/app-configuration.md)；理解请求与执行者见
[I/O 执行](../guides/io-execution.md)；构建 MCU 镜像见
[第一个 F407 工程](first-f407.md)。安装包消费见
[使用 Source SDK](../guides/use-source-sdk.md)。
