# XPT2046 原始触摸采样

`Nexus::XPT2046` 是可选、无动态分配 C11 组件，维护精确差分 12 位协议。输入为
已启动同步 SPI endpoint、低有效 live PEN 谓词及有身份的单调 clock。endpoint
必须提供 mode 0、MSB-first、完整 24-clock 事务，以及适合模块的真实 settling／rate。
启明 PA5／PB1／PB2 不是同一硬件 SPI 路由，因此用软件 SPI；引脚和 PF11 PEN 事实
属于 Board／assembly。

用户确认模块为 ILI9341＋XPT2046。本组件不绑定或 reset LCD、不重复取得 GPIO
lease、不配置 IRQ、不校准或旋转坐标、不滤波，也不创建 polling worker。调用者
串行管理 device／endpoint／PEN，并在释放存储前会合所有 observer。

PEN 必须二选一：单 bit mask 的 GPIO face，或 mask=0 的 EXTI face。EXTI 须实现
可选实时电平读取方法；缺失时在首次 SPI 写前拒绝。这样 PF11 由一个 EXTI owner
管理，不创建冲突的第二 GPIO owner。不会消费 buffered edge 来伪造当前电平。

## 精确命令与结果

初始化完成一次 `0x90 00 00` 转换。采样先检查实际 PEN，分别发送独立
`0xD0 00 00`（X／channel101）和 `0x90 00 00`（Y／channel001）。可选 pressure
另外发送 `0xB0`（Z1／channel011）与 `0xC0`（Z2／channel100），各跟两个零 byte。
全部命令选择 differential 12-bit、PD1:PD0=00：reference off、转换间 power-down、
转换完成后 PENIRQ enabled。不提供未声明的 alternate controller 或 ADS7846 回退。

每次 3-byte 返回为一个 null bit、12 个转换 bit、3 个 trailing zero；short 或畸形
帧返回 IO。整组结束再读 PEN，只有前后均低才发布复制样本。pen-up 返回 EMPTY，
不修改输出。转换彼此顺序进行，两次低电平或 IRQ 不证明同时转换或触点稳定。
Z 是原始数值，不计算电阻。timestamp 是配置非零 domain／epoch 内的软件完成观察，
uncertainty 未知，不是精确物理触点时间。

## 有限窗口与不确定绑定

调用者的一个有限 deadline 约束整次调用。每个独立 24-clock exchange，包括 init
和 stop 的 PD=00，都另外取调用者 deadline 与 exchange start＋1600 µs 的较早值，
以检查溢出的有限加法得到；完成时到达或超过边界即拒绝。

此保守完整交换窗口包含 command、acquisition 和 transfer 开销，强于手册第 26 页
针对可中断 16-clock conversion 的 1.6 ms sample-and-hold droop 提示，不猜内部
hold 瞬间，也不修改其他 SPI 用户的通用 edge-gap 合同。每个 X／Y／Z 转换各有
窗口，不要求全部转换落在同一 1600 µs 内。

endpoint 接收局部 deadline，返回后再读取 clock，即使 endpoint 忽略 deadline
而晚返回 SUCCESS 也拒绝发布；endpoint 的实际错误仍优先。clock regression／
exhaustion、局部加法非有限均拒绝。每个 transport 返回，包括错误，都结束局部数组
访问；仅 SPI owner 可保留自身 pin 恢复责任。

失败／short transfer 关闭 sampling，保留相同 device binding 为 uncertain。必要时
先恢复 SPI owner，再用相同只读 config 重试初始化。stop 发送另一完整 PD=00，
仅成功后解绑，可能需独立 cleanup deadline。timeout 或 GPIO 看似 idle 不是
converter shutdown 证明；不同 config 不能替换 uncertain binding。包括首次 clock
读取的全部 callback 都防止重入。

## 来源和验证

XPTEK **XPT2046 Touch Screen Controller**、copyright2007、30 页，Tables4／5
定义 channel，page21 定义 24-clock transfer，page22 定义 control，pages24／25
定义 PD／PENIRQ：

- <https://files.waveshare.com/upload/9/98/XPT2046-EN.pdf>
- PDF SHA-256：`d01b61c1f08dc74963ac73d996c93ed0aed485ba33dcb09960adbbb629e9d515`

这是由模块供应商镜像取得的厂商 PDF，不宣称来自当前厂商站点，也不证明已焊 silicon
身份或器件在线。page26 明确转换须在 1.6 ms 内完成以避免传输中断导致保持电压下降；
page27 Table9 的 DCLK 高／低最小时间不能转为实测频率或波形保证。

GoogleTest／GoogleMock 执行真实命令、掉电、12 位解码、pressure 选择、pen-up／
中途松手、partial／framing failure、错误输出不变、单调 deadline、callback 重入、
保留清理责任及 GPIO／EXTI source／alias 拒绝。物理 acquisition settling、PEN
行为、SPI 波形、坐标准确度与整模块 HIL 仍未执行。
