# 选定 CMSIS-DSP f32 profile

`CMSISDSP.cmake` 定义惰性的 `nexus_add_cmsis_dsp()`；`Select.cmake` 显式调用它。
选中后通过统一 `nexus_component_source(cmsis_dsp, ...)` 校验组织托管的锁定源码，
不下载依赖、不扫描或编译其他库存。源文件闭包只维护在该 CMake 列表及来源锁中。

本 profile 实际选择 f32 dot、scalar FIR、DF1 biquad、RFFT1024 及其 CFFT、
bit-reversal 和常数表依赖。Native 采用 upstream 的 `__GNUC_PYTHON__` portable
编译路径；Cortex-M4 使用实际 CMSIS-Core。未选 Helium、Neon、f16、定点滤波、
任意长度 FFT 或 CMSIS PID；没有替代实现或返回 UNSUPPORTED 的空壳接口。

`Nexus::CMSISDSP` 的供应商头和编译宏保持私有。
`Nexus::AlgorithmsCMSISDSP` 通过 `nexus/algorithms/cmsis_dsp.h` 暴露 Nexus 类型，
实例和全部历史由调用方提供，不保存供应商 Handle。

FIR 系数按 upstream 约定倒序，history 至少 `taps+maximum_block-1` floats；
DF1 biquad 每节 5 个系数和 4 个历史，反馈系数在差分方程中为正。
这两个 block kernel 直接提交历史与输出，调用方负责 f32 动态范围，不能假定
计算溢出具备 portable SOS 那样的事务回滚。

RFFT 固定读取 1024 个实数；调用方分别提供 1024 个输入、可写工作区和频谱 float。
输入先复制到工作区，输出按 `[DC, Nyquist, Re(1), Im(1), ...]` 打包，正向不归一化。
采样率、窗函数、功率谱标度和物理单位由使用者提供。固定长度初始化避免调用任意
长度入口拖入不需要的变换表；实际链接大小仍需对应 ARM 镜像验证。

测试执行真实 upstream 代码，检查非对称 FIR 系数、biquad 符号、常量 DC 和交替
信号 Nyquist 分量。源码存在或 profile 可配置不等于已经执行或满足硬件时限。
