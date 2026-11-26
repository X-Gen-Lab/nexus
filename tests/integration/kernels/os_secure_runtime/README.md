# 真实 split-world 编译／链接合同

两幅独立生产 Runtime：Secure 选明确 CPU facts、baremetal、`Nexus::SecureContext`；
NS 选匹配 Nonsecure facts 与 split FreeRTOS。Secure 链接生成七个不同 SG veneer
于 `.gnu.sgstubs` 和真实 CMSE import object，NS 消费该 object，保留真实 SVC／
PendSV context 路径。

例 task 与 Idle 都有 caller-owned NS TCB／stack 和各自已准备 Secure metadata／
stack；不可变 selector 只使用两组明确配对的合成 TCB 地址。两镜像都无默认 Secure
context pool 或 heap。

ROM／RAM／NSC 范围、TCB 身份为软件事实，没有 SAU／IDAU／vector／IRQ target
初始化、外部 NS transfer、板级启动或硬件执行。真实集成另供电气、内存归属和启动
合同，实板验收保持待执行。
