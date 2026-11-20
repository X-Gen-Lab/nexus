---
id: guides.board-porting
type: how-to
audience: maintainer
source_language: zh_CN
---

# 为已有芯片接入 Board

仅在完整料号已有 provider 时走这条路径。Board 增加 PCB、晶振、电源、引脚、外部
器件和安全初始电平事实，不复制 HAL，不授权芯片的全部可用路线。

## 准备最小事实

记录完整料号和封装、PCB／原理图／BOM 版本、来源、晶振、供电、SWD 预留、跳线、
引脚与有效电平。先接一个状态输出和一个串口；未观测的修订、桥接与电气条件保持
未知。晶振值不能自动合成新的 PLL、Flash latency 或外设内核时钟方案。

公共可复用 Board 位于 `boards/`，私有 PCB 包由产品仓库维护。普通包只需
`board.json` 和简短 README；无法由现有数据表达的电气行为才增加受限的 C hook。

```{literalinclude} ../../boards/stm32f407_qiming_v31/board.json
:language: json
:caption: 维护的完整 Board 输入；文档来源与实际 PCB 观测分开。
```

## 创建外部装配

以下为命令模板；先将包路径、时钟和 binding 替换为实际审核的值：

```sh
python "$NEXUS_SOURCE/tools/configure/configure.py" init \
  --board-package /path/to/private-board --backend baremetal \
  --clock REVIEWED_CLOCK --profile gpio --binding REVIEWED_BINDING \
  --instance status --output /path/to/product/assembly.toml
python "$NEXUS_SOURCE/tools/configure/configure.py" provider-check \
  --assembly /path/to/product/assembly.toml --json
```

`--board` 与 `--board-package` 二选一。命令行包路径相对工作目录，生成 TOML 中的
包路径相对该 TOML，移动整个产品目录可保留关系。路径不能通过 symlink 逃逸。
`provider-check` 解析同一 typed IR 并生成实际静态 bundle，记录所选 part、Board、
后端、控制器与输入身份；此时尚未编译。

## 编译完整消费者

在自己的完整 CMake 应用中选择该 `assembly.toml`，按[CMake 接入](cmake-consumption.md)
使用所选源码和 ARM 工具链。可先复制维护的对应 F407 starter，只修改自己的 Board
引用与实际选择，保留应用源和静态存储：

```sh
cmake -S /path/to/product -B /path/to/product-build -G Ninja \
  -DNexus_DIR="$NEXUS_SOURCE/cmake" \
  -DCMAKE_TOOLCHAIN_FILE="$NEXUS_SOURCE/cmake/toolchains/arm-gcc.cmake"
cmake --build /path/to/product-build --parallel 2
```

核对实际 part、向量、IRQ、链接 RAM／Flash 和资源报告。裸机与 FreeRTOS 使用独立
构建目录。配置拒绝时修改作者输入，不改生成输出或静默选别的引脚。

Board 字段与绑定格式见[接入合同](../reference/contracts/integration.md)。实际接入
设备后再按[HIL 流程](performance-hil.md)绑定 PCB、part／UID、探针、接线、镜像和
测量；源文件中的硅片路线不能证明连接器连通性或电气安全。
