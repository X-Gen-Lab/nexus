# WM8978 选定播放控制

`Nexus::WM8978` 是 C11 的精确 codec 控制组件，通过已有 settled I2C endpoint
访问 WM8978 的 9-bit data／7-bit register 两线控制接口。它不访问 MCU 寄存器，
不取得第二份 I2S／DMA／IRQ owner，也不把 codec 控制传输当作 PCM 数据传输。

当前选定范围为外部 MCLK／256、48 kHz nominal、slave I2S、16／20／24／32-bit、
stereo DAC 到 OUT1 headphone，无 1.5x boost。ADC／mic、speaker OUT2、PLL、EQ
和录音未选择。接口宽度仍须与所选 MCU audio provider 分别验证；F4 I2S 不能因
codec 支持 20-bit 就自动获得该 wire mode。

## 外部输入

TOML 选择已经审核的 I2C／音频控制器与 endpoint，应用直接链接目标并提供：

- 静态、清零 `nx_wm8978_t` 与不可变 `nx_wm8978_config_t`。
- 地址 0x1a 的已启动 endpoint、稳定电源和实际 MCLK。
- 明确 MCLK 事实与允许误差，当前 profile 为 12.288 MHz 的 0～1000 ppm 范围。
- 外部 VMID capacitor 对应的正数 settling 上界；组件不猜测一个通用毫秒数。
- 原始单调 clock、有限绝对 deadline 和唯一普通执行者。

WM8978 控制口是 write-only，写入成功／I2C ACK 不能证明模块型号或实际模拟输出。
PCB、负载、供电、XCLK／MCLK、I2S 接线和时序保持独立事实。启明 V4.1 目前只
声明可选 WM8978 控制 GPIO，不能据此声称完整音频数据线已接通。

## 运行顺序

```text
外部启动电源和 MCLK，PCM 保持停止
  → initialize：reset、mute、启用 DAC／mixer、VMID charging
  → 调用者等到 ready_at 后再次 service
  → 配置 bias、OUT1、I2S format 与外部 MCLK，仍保持 mute
  → 产品核对 PCM 格式、选择 gain、启动数据，再显式 unmute
  → 退出时 mute、join PCM/DMA/borrowers、stop codec
  → 原 owner 最后归还 I2C、MCLK 和 Platform
```

service 不 sleep 或 spin，VMID 尚未到时返回 BUSY，整个启动共用原 deadline。
默认 gain 为 -57 dB 且静音；左右 gain 为 -57～+6 dB、1 dB 步长，先写左 shadow，
再用右 HPVU 一起更新。写入成功不代表音频采样、模拟 settling 或 zero-cross 已实测。

控制或超时失败保留 UNCERTAIN 与同一 config。部分 stereo shadow 不能被当作完整
更新；调用方先恢复原物理 owner，再显式 stop／同绑定重建。clock rollback 永久
锁存该会话的时钟错误，增加 deadline 或恢复更大的数字不能修补原时间域。

stop 先静音，再依手册顺序关闭 R1、R2、R3。失败保持绑定，成功只证明控制调用
结束，不证明 VMID 已放电、外部供电已关闭或其他 PCM buffer 已退出。

## 验证与资料

依据 `references.json` 记录的 WM8978 production datasheet Rev 4.5，选定模拟通路
和控制序列直接用 C 实现。`tests/google/components/wm8978_test.cpp` 运行生产 C，
覆盖真实 wire encoding、VMID 分阶段、stereo latch、mute、短传输、部分失败、
deadline 和 clock rollback。测试模型不证明音质、爆音抑制或实际 I2S 电气行为。
