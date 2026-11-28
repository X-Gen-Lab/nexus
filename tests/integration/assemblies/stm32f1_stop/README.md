# STM32F103RB PC13 STOP 软件参考

`f103rb-stop.toml` 选用 HSI8、裸机和 PC13 下降沿，按 MB1136 C-03 的 B1 接线声明，
生成专用 EXTI13 实例。启动后通过强类型工厂取得 `exti.wake`。
编译器仅为选入 `stop-wake` 的装配加入 STOP 和时基重建代码。
参考固件默认走普通启动和停止，`stop_requested` 由工装显式设置。

调用方先收回旧 deadline、清理已有 EXTI 事件，加入所有申请方并关闭
自己的 ClockDomain。随后 `nx_stm32f103_stop_enter()` 完成正常 Platform 挂起，
保留指定的自主唤醒实例。其他资源必须撤回。存在旧 pending 时返回 BUSY，
原标志留给其所有者。入口拒绝已启用的 RTC、看门狗和其他外设，
验证 PWR 与 EXTI 状态，保持 Flash 接口时钟，使用 WFI。

TIM2 在进入前退休并保存旧纪元，确认 HSI8 RUN 后发布新纪元，
外部 IRQ 随后才获准在新纪元执行。只有 `resume_ready` 为真，
应用才重新开放 ClockDomain 并调用 `nx_platform_resume()`。
如果 Platform 已挂起但 RUN 无法恢复，保持 admission 关闭并进入产品
恢复流程。返回的 `wait_issued` 和 `wake_pending` 可用于诊断，
停留时间与物理 STOP 状态由实板验证。

依据 RM0008 Rev21 §5.3.4、表 14、§10.3，以及 ES096 Rev15
§§2.2.1、2.2.3、2.2.4。
