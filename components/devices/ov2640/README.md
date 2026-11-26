# 官方 OV2640 选定 RGB565 控制

显式调用 `nexus_add_ov2640()` 并链接 `Nexus::OV2640`，复用组织托管的 ST 官方
`ov2640.c` 原始初始化表。公共头只使用 Nexus 类型；官方 BSP Common 和 camera
类型、传统 `CAMERA_IO_*` 回调均保持私有。回调符号在叶 target 内命名隔离，避免
其他 camera component 争用旧 BSP 的全局名称。

## 来源与兼容组合

`dependencies/ov2640.lock.json` 固定官方 driver，
`dependencies/stm32-bsp-common.lock.json` 固定兼容的 BSP Common `v4.0.1`。
新版 Common 已采用不同 camera ABI，不能只换到最新版本。旧版本原 BSD-3-Clause
notice 位于 `camera.h`，完整保留；构建只复制其原字节到私有 Common 布局，不修改
vendor 文件、不伪造缺失头，也不生成寄存器算法。

## 范围与装配

选定上游四套完整 RGB565 table：160×120、320×240、480×272、640×480，DVP
8-bit、low byte first。产品显式选择 resolution、实装模块的 expected PIDL、
稳定电源／reset、外部 XCLK、SCCB 与 capture 的实际路线。datasheet 1.6／2.2
PIDH 为 0x26、PIDL 为 0x41；其他 revision 不能通过自动 fallback 混入。

F407 首个软件参考采用 QQVGA 160×120，完整 RGB565 frame 为 38,400 字节。
QVGA 为 153,600 字节，超过 128 KiB 主 SRAM；CCM 不可 DMA，CPU-only 外部 SRAM
窗口也不自动获得 DMA 权限。更大尺寸必须有单独审核的存储和带宽计划。

配置声明的 XCLK 为本 profile 选择的 6～24 MHz 范围，实际来源和 duty／电气条件
由产品证明；组件不建立帧率或 PCLK 上界。启明 V4.1 当前只授权 camera-scl／sda
控制线，不能把排针或软件 table 当作 DCMI 数据线接入证据。

`nx_ov2640_port_t` 是 settled SCCB register8/data8 与有界 wait 端口。可以直接把
`nx_software_sccb_write_register`／`read_register` 连接到已有 exact child，
write ID 为 0x60、read ID 为 0x61。SCCB 使用既有独立 personality，不能将
Don't-Care 位解释成宽松的 I2C ACK。应用提供已有执行者和同步 wait，原 deadline
贯穿所有回调；每次返回都结束输出参数借用。

## 生命周期与失败

initialize 先运行官方 ID 访问，核对完整 PIDH／指定 PIDL，再执行实际 vendor table，
最后读回 RGB565 mode。原始 driver 的 reset 200 ms 与每寄存器 wait 都由外部
有界 wait 实现；只在 startup／maintenance 普通任务调用，不放入控制 ISR。

官方 void callback 不能直接返回错误，桥接保存第一个失败并屏蔽所有后续硬件和
wait 访问。有限表剩余循环只做有界检查，最终真实错误返回并保留 UNCERTAIN config。
不会把表执行到末尾误判为成功，也不在失败后自动重放。

capture／DCMI／DMA 继续使用已有公共 I/O，与 sensor control 各有一个明确 owner。
应用先停止 capture、join request／IRQ 和 frame consumers，再执行 sensor stop。
stop 为官方 driver 缺失的窄 C 补充：保留 COM2 drive bits、进入 standby 并读回。
失败保持同一 binding；成功不关闭 XCLK／供电、不归还 SCCB，也不撤销其他 loan。

`tests/behavior/components/devices/ov2640_test.cpp` 实际编译官方 driver、兼容 Common 与桥接 C。
外部模型仅提供 SCCB、时间、wait；覆盖 table、exact identity、void callback
错误、deadline、停止失败和独立实例。软件结果不声明 gapless video、JPEG、物理
图像质量或 HIL；JPEG DMA padding 也不能作为码流长度。
