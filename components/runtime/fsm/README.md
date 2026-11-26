# 同步有限状态机

`Nexus::FSM` 提供 caller-owned 的平面状态机。调用者提供只读 state 和 transition
表、实例上下文及串行执行者；组件不创建任务、队列、定时器、全局 tick 或堆对象。
公共入口为 `nexus/components/fsm.h`。

## 选定软件范围

本批 RUN-01 选择平面、同步、单事件运行到完成的 profile。初始化验证整表并执行
初始状态 entry，事件为 NULL；空闲重置不执行旧状态 exit。按表顺序查找匹配 source
和 event 的行，guard 返回 false 时继续。外部转换执行 exit、transition action、
entry，外部自迁移也执行三者；内部转换只执行 action，且目标必须等于源。

exit 和 transition action 观察源状态，entry 观察目标状态。未匹配返回 EMPTY，
同实例回调重入返回 BUSY；回调无错误返回，也没有撤销回调外部效果的承诺。所有
回调须有界且不阻塞。独立实例可以共享只读表，不能共享可变实例上下文而不另行串行化。

RUN-02 层次状态机为 `not_selected`：本 profile 不实现父状态回退、祖先链 exit/entry、
历史状态、正交区域、自动/完成转换或 UML 状态图完整语义，也没有相应空壳 API。
确有需求时单独选择生成 C 或经审核 HSM，并用祖先链及自迁移合同验证选定语义；
不能将平面 FSM 的存在描述为已经实现 HSM。

## 生命周期与时间

配置、表和上下文存活至最后一次调用返回。实例首次使用前清零，调用者串行化初始化
和分发。事件及 payload 仅在本次 dispatch 期间借用，不保存。异步消息复用已有 queue，
连续载荷复用已有 stream 的存储合同；组件不另建 event bus。超时由外部 Core
deadline/周期辅助函数变为事件，ClockDomain 可用于外部域生命周期协作，不是强依赖。

GoogleTest/GoogleMock 位于 `tests/behavior/components/runtime/common_fsm_test.cpp`，直接验证
真实 C 的转换顺序、guard、重入拒绝、实例隔离和非法配置。构建、ARM 链接与物理执行
分别由对应结果建立；FSM 不证明用户回调 WCET 或业务状态安全。
