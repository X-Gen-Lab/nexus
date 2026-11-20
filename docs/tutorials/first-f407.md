---
id: tutorials.first-f407
type: tutorial
audience: consumer
source_language: zh_CN
---

# 构建第一个 STM32F407 镜像

本教程使用启明 STM32F407ZGT6 V3.1 的裸机模板，构建 GPIO／UART 问候应用。
完整料号、Board 与时钟选择均来自维护模板。本教程的成功出口是镜像及资源报告；
实际下载与串口观测另行进入板卡流程。

## 准备源码、工具与所选依赖

按[源码指南](../guides/source-checkout.md)取得平台及示例。需要 Python 3.11+、
CMake 3.31+、Ninja，以及平台锁定的 ARM GCC；编译器目录加入 `PATH`。

```sh
NEXUS_SOURCE=/absolute/path/to/nexus
NEXUS_EXAMPLES=/absolute/path/to/nexus-examples
NEXUS_EXAMPLES_REVISION=REPLACE_WITH_SELECTED_40_CHARACTER_COMMIT
PRODUCT_DIR=/tmp/my-nexus-f407
python "$NEXUS_SOURCE/tools/dev/dev.py" prepare --scope preset \
  --preset stm32-qiming-baremetal --allow-network
```

该准备入口读取精确 preset／TOML，裸机最小路径只选择 CMSIS Core、Device F4 和
HAL F4。GoogleTest、完整 Native 维护图和未选中间件不属于这条消费路径。

## 创建并构建

`PRODUCT_DIR` 必须尚不存在。先从固定提交创建 `qiming` 模板：

```sh
python "$NEXUS_SOURCE/tools/dev/dev.py" create \
  --templates "$NEXUS_EXAMPLES" --revision "$NEXUS_EXAMPLES_REVISION" \
  --template qiming --source "$NEXUS_SOURCE" --output "$PRODUCT_DIR"
cmake -S "$PRODUCT_DIR" -B "$PRODUCT_DIR/build" -G Ninja \
  -DNexus_DIR="$NEXUS_SOURCE/cmake" \
  -DCMAKE_TOOLCHAIN_FILE="$NEXUS_SOURCE/cmake/toolchains/arm-gcc.cmake"
cmake --build "$PRODUCT_DIR/build" --parallel 2
```

模板选择 `stm32f407_qiming_v31`、`hse8-pll168`、状态 GPIO 和 USART1。
应用角色为 `status`、`console`，通过生成的强类型 factory 取得既有接口。
成功时固件目标生成 ELF、BIN、map 与 `*.resources.json`；资源报告应为通过，
且 part、Board 和配置输入与所选 TOML 一致。

## 核对与后续操作

阅读生成工程的 README 和对应 Board README。UART 预期为 115200 baud、发送
`Nexus starter ready`；这些是应用意图，尚未实测的 PCB 跳线、时钟、电平与真实
串口输出仍保持未知。当前实板 HIL 状态是 `not_executed`。

其他维护模板为 `sky`、`qiming-core`、`qiming-v41`。切换模板时同时核对完整料号、
晶振、Board 绑定与实际 PCB，不只改 CMake 名字。不同装配使用独立构建目录。

修改配置见[应用配置](../guides/app-configuration.md)，新 PCB 见
[Board 接入](../guides/board-porting.md)，常见失败见
[排错指南](../guides/troubleshooting.md)。
