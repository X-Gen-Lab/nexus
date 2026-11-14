# 最小公开 GPIO 消费工程

这是平台公共接口的验证资料，不包含产品任务、协议或私有 PCB 数据。
`create --public` 复制这些文件，并按显式 Board、clock 和 GPIO binding 生成
`assembly.toml`，再由所选平台自己的配置工具验证。无需私有示例仓库。

工程固定使用 baremetal，不创建 scheduler、任务、IRQ 工作队列或 Native 模型。
`main.c` 启动平台，通过强类型 factory 取得 `status`，读取授权 GPIO snapshot，
然后停止平台。不推断引脚、LED 极性或掩码，不执行闪灯应用逻辑。
GPIO 配置使用所选 Board 的初始安全电平；它不是板级电气验证。

运行创建命令输出的 configure 和 build 命令。构建输出 ELF、BIN、map 和资源报告；
默认不烧录或执行硬件。`.nexus-template.json` 记录所选源码或 SDK、配置验证和
复制文件摘要，不建立软件发布或物理资格。保持 `assembly.toml` 和应用源码为
工程拥有的输入；不要直接修改生成的绑定。
