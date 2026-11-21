# 显式状态的可选算法

`Nexus::Algorithms` 提供 C11 scalar profile：饱和 Q15/Q31 乘法与转换、
CRC／字节序、流式统计、EMA／滑动均值／中值、FIR／SOS、分段线性插值、
积分／滤波微分／斜率限制及 PI／PID。公共声明位于
`include/nexus/algorithms/`；它们是行为合同的唯一入口。

## 存储、单位与执行

调用方拥有每实例状态、系数和工作区，并串行化状态更新、reset 与生命周期。
只读系数可共享，滤波历史、控制积分和工作区不可共享。输出、工作区与控制／输入
存储按各头文件合同保持分离；这些接口不验证任意无效内存是否可读写。
算法不创建任务、队列、堆、全局时钟或设备访问，不自动接入 factory。

周期用秒或微秒的显式命名表达，PID 的 Ki／Kd 是连续增益，不是已经乘过采样周期
的离散系数。采样节拍、丢样、实际执行器限制、时钟 epoch 切换和故障处置由外层
owner 提供。浮点 profile 要求编译器保留 IEEE 非有限值检查，不能用假定所有输入
均有限的 fast-math 选项重新解释其拒绝合同。

## 已选能力及边界

| 能力 | 当前 profile | 仍由调用方决定 |
| --- | --- | --- |
| 数值 | Q1.15／Q1.31 乘法，最近舍入、半数远离零，端点饱和 | 缩放、单位和定点动态范围 |
| CRC | 显式 8..32 位模型、分段 update／finish，ISO-HDLC／MODBUS／SMBUS 模型 | 线上格式、是否校验和硬件 CRC 选择 |
| 统计 | 双精度 Welford 方差和均方递推，明确总体／样本除数 | 窗口、异常样本规则和测量可信度 |
| 轻量滤波 | EMA、O(1) 补偿求和滑动均值、有界奇数中值窗口 | 采样率、滤波目标和启动时间 |
| FIR／SOS | f32 数据，double 中间量，caller workspace | 系数设计、频率响应、量化和实际预算 |
| 插值 | 一维分段线性，拒绝／夹持／外推三种明确边界 | 标定数据与 Settings／Storage 接入 |
| PID | measurement D、条件积分抗饱和、实际输出回算、track、保输出改参 | plant 稳定性、调参、硬件限制和手自动模式 |

portable FIR 的系数顺序是当前样本系数优先；portable SOS 使用传统分母
`1+a1*z^-1+a2*z^-2`，初始化检查归一化实系数二阶节的严格稳定条件。
SOS 每步先写独立工作区，全部成功后才提交活历史；失败可能改变工作区。
这些约定与 CMSIS 的系数顺序或反馈符号不能混用。

PID 的条件积分阻止继续向饱和方向累加；外部限流／限速后的实际执行值可显式回算。
`track` 将积分配平到当前输出并清零 D；所需积分超出限制时拒绝，不能承诺任意
输出都能平滑接管。改参保持最近已算输出，之后的积分和 D 动态仍遵从新参数。
软件算子通过测试不证明某个实际 plant 的闭环稳定性。

双线性插值、拟合、姿态估计、重采样、自整定或运动轨迹没有占位 API。
存在真实复用需求时，以独立的数值合同、内存预算和参考向量增加。

## 条件 CMSIS-DSP profile

重型后端只有显式调用 `nexus_add_cmsis_dsp()` 或选中其 `Select.cmake` 才验证来源、
定义和编译 target；普通 Algorithms 不携带第三方头或库。
`Nexus::AlgorithmsCMSISDSP` 暴露 Nexus 窄接口，实际调用
`Nexus::CMSISDSP` 中锁定的 upstream f32 dot、FIR、DF1 biquad 和固定 RFFT1024。
详见 [selected profile](cmsis_dsp/README.md)。

`tests/google/components/common_algorithms_test.cpp` 是真实 C 算法合同；
CMSIS 的真实源码与系数／频谱约定由独立 `common_cmsis_dsp_test.cpp` 检验。
主机结果不建立 MCU 周期预算、物理采样精度或控制对象资格。
