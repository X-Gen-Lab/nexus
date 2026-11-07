# ILI9341 同步 16 位并行显示

`Nexus::ILI9341` 只维护 ILI9341 命令协议，不拥有 Board、并行控制器、复位电源、
背光、帧缓冲、任务或 scheduler。target 显式选入，未引用代码不驻留固件。调用者
提供全部状态与像素存储；只读配置保存稳定 `nx_parallel16_endpoint_t` 和 clock，
实例共享只读方法表，不引入 SoC 或厂商类型。factory 查询不取得 bank。

## 窄端口与协议

同步 settled 端口在每次返回前完成所有访问并释放每个 span，包括错误返回；不得用
会保留栈 span 的异步 submit 伪装此端口。当前原生配置为 240×320 RGB565，MADCTL
的 MV 交换逻辑宽高，只接纳 MY／MX／MV／BGR。矩形须完全在界内，半字数恰好等于
宽乘高，不隐式裁剪、复制、格式转换或分配 framebuffer。完成像素前缀表示总线写入，
不证明可见像素或无撕裂。

初始化读取 ID4，随后发送 SWRESET、SLPOUT、像素格式、方向和 display-on。不驱动
共享 NRST，不猜测 panel 专有 power／gamma；调用前确保供电和外部复位稳定。未知
或 warm 控制器可能已 Sleep Out，因此 SWRESET 后保守等待 120 ms 再 SLPOUT，之后
再等 120 ms。service 每次推进一个阶段，不睡眠或忙等。有限初始化 deadline 覆盖
全部访问与两段等待；失败 reset／sleep-out 尝试仍建立安静区间，stop／恢复不能提早
发送下一条命令。

## 失败与退出责任

总线访问前的拒绝保持输出不变；一旦尝试命令，传输、数量或时间错误保留相同绑定为
`UNCERTAIN`。显式 stop 在安静时间后发送 display-off，成功只释放组件绑定。调用者
再会合并释放 bank endpoint，最后停止共享 bus。timeout 不允许另一个 owner 回收
transport，背光与电源策略仍在外部。

## 选定 F407 集成范围

以下是精确 F407 FSMC 参考，不是通用组件要求。完整平台／接线入口见
[F407 SoC](../../soc/stm32/stm32f407/README.md)与
[启明 V4.1 Board](../../boards/stm32f407zg_qiming_v41/README.md)。

私有 owner 维护运行期 NE3 一 MiB SRAM window 与 NE4 command／data port，使用
一份 AHB3 gate receipt 和外层 Platform 的单一 GPIO snapshot lease。LCD／SRAM／
两者的 pad 并集分别为 20／40／41，各 bank 保留独立 read／write timing。

单独选择的 reset-only SRAM plan 可在 C runtime 前初始化外部 DATA／BSS。运行期
接管检查原 Flash descriptor 与实际寄存器；boot pad 没有普通 restore snapshot。
静态 boot receipt 在 Platform 启动前已保护全局对象，拒绝接管也不清除其责任。
启动 backing 保留到 reset，即使没有 endpoint loan，bus stop 仍在操作 bank／gate
前返回 BUSY。该模式拒绝整 bank runtime loan，避免覆盖已放置全局对象。普通 SRAM
CPU load／store 没有逐次虚调用，但 CPU 可访不证明 DMA 可达或器件已焊。

16 位 LCD 选择 NE4 和外部 A12，命令为 `0x6c000000`，数据为 `0x6c002000`；外部
地址相对 CPU 字节地址左移一位。PIO 每 32 words 及最终 word 后检查有限单调
deadline，观察最多可滞后这一有界批次。不推导 DMA、连续图像、光学时序或实物资格。

协议依据为 ILITEK *ILI9341* 的 01h、11h、28h、29h、2Ah／2Bh／2Ch、36h、3Ah、
D3h 命令及 reset／sleep-out 恢复限制。只使用文档事实，不分发参考 PDF。
