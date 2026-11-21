# 只读 letter-shell 服务

`nexus_add_shell()` 选择锁定的真实 letter-shell parser，提供 `Nexus::Shell`。
`Nexus::ShellStorage` 仅在静态装配点提供精确 profile／layout，避免猜测最大 opaque
存储；普通消费者使用不含 vendor 类型的接口。未选库存不核验、不编译、不初始化。

应用提供同步 read/write 回调、line/history buffer、执行上下文、输出 deadline 和
退出顺序。`nx_shell_step` 最多读取处理一个字符，回车在同一任务同步执行命令；不
创建任务或隐藏循环。ISR/DMA 接收路径仅向已有 RX 存储发布字节；上游 README 的
ISR 直接执行 parser 方式不属于此 port 合同。

当前 profile 只有一个 live shell、两条历史和显式只读命令表：`help`、`version`、
`health`、`fault`、`reset`、`resources`。上游把缓冲分成三份，不会全部用于一条命令。
非阻塞入口 guard 管理短暂 callback context，不发现全局 transport。默认关闭地址
执行、变量导出／写入、函数签名数组、heap、companion 和上游格式化输出；固定诊断
文本及数值使用与 Log 共享的有界无堆 renderer，不调用 libc formatter。

命令读取调用方发布的快照及不可变版本文本，不推进 I/O、不解引用 live request、不
清故障记录、不改参数、不擦 Flash、不喂狗，也不复位硬件；`reset` 只报告已捕获原因。
缺失数据明确输出 unknown。产品认证、维护授权和危险命令仍属于外部产品工程；上游
权限位及只读默认用户的空密码不构成安全边界。

当前 profile 使用显式上游命令表，无需 shellCommand section／KEEP。产品选择另一种
命令导出 profile 时，须单独处理各编译器的 section 保留、静态库抽取与 LTO 验证。
