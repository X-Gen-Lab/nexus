---
id: guides.component-porting
type: how-to
audience: maintainer
source_language: zh_CN
---

# 接入通用器件或协议组件

外部器件和协议通过公开 transport、time、storage 等端口使用平台。客户私有流程、
寄存器策略、产品 worker、分区和恢复决策留在外部工程。

## 先定职责，再写适配

1. 明确组件是否是值得复用的算法／协议，固定输入输出、容量、单位和错误。
2. 定义窄端口，不依赖 Board、SDK Handle 或具体 UART／SPI provider。
3. 按需求提供独立可选 adapter；parser 不隐式打开外设、分配任务或修改时钟。
4. 用实际业务行为、无效输入、容量耗尽和失败退出测试组件，不只测 mock 调用次数。
5. 以完整消费者验证生成头、公开 API、ABI 与链接范围，再评估实际资源成本。

## 放置与连接

组件按 runtime、buses、devices、services、protocols 分类，叶目录维护代码和 CMake
依赖。普通库可直接连接公开 target，无需新增 TOML 类别。

确需静态绑定或供应方选择时，在 `components/catalog.json` 登记逻辑 ID、叶入口和
公开 target；不在中央调度器加组件名字分支。叶目标分别声明生成公开头的 API
使用要求及 bindings 私有编译要求，避免向应用泄露私有目录。

## 与硬件组合

TOML 选择 transport 控制器与 endpoint，应用选择逻辑角色、执行频率、队列和静态
工作区。组件不把第一个 SPI／I2C endpoint 当成整个控制器。

以 RS485 为例，Board／产品描述收发器、DE／RE、电平和接线；SoC UART provider
处理其维护的传输终态；adapter 决定何时切方向。不能用固定软件延时替代真实线上
完成，也不能从普通 UART 成功推断整个物理协议已验证。

目录归属见[组件说明](../../components/README.md)，端口与公开签名见
[组件 API](../reference/api/components/index.md)，执行责任见[I/O 执行](io-execution.md)。
