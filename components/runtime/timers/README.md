# 有界 deadline 容器

`Nexus::Timers` 使用精确 caller-owned slot 数量，采用 O(capacity) 扫描。
它不创建 heap、callback、worker 或 OS timer；owner 显式传入 Core 时间并逐个
pop due event，再经已有 FSM、queue 或调用路径分发。

同期限按 arm 顺序出队。ticket 是 slot+epoch，取消旧 ticket 不能影响重用 slot；
epoch 或顺序耗尽不回绕，容量满不覆盖旧 timer。`next_due` 只查询，不读取时钟。
过去的有限 deadline 可以 arm，并在下次符合时间域的 poll 中到期。

时钟域改变时 owner 等待退出旧消费者并 reset。reset 取消全部活项，但保留身份计数；
不能通过清零活容器回收尚可访问的 ticket。tag 只是复制的整数，若编码指针，其对象
存活期仍由调用方负责。公开头规定输出独立、唯一执行者和失败行为。
