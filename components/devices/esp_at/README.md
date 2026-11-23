# ESP32-C3 单连接 ESP-AT profile

`Nexus::ESPAT` 是选定 **ESP32-C3-MINI-1、ESP-AT v4.1.1.0** 的 C11 模块协议。
它复用现有 `Nexus::ATClient`，通过复制语义 `nx_byte_stream_t` 接收已拥有的 UART。
调用者提供元数据、line、command、网络 TX／RX 缓冲和一个显式执行者；组件没有
任务、堆、UART controller owner 或自动命令重放。

## 构建与依赖

```cmake
target_link_libraries(application PRIVATE Nexus::ESPAT Nexus::UARTByteStream)
```

普通组件直接链接公开 target。UART 由外部 TOML 与 typed factory 选定，模块的
SSID、凭据、host、端口、供电、reset、UART 引脚／速率／流控和恢复策略由产品给定。
UART request 到复制字节的连接见 [UARTByteStream](../../runtime/uart_stream/README.md)。
协议代码不包含 STM32 Handle，也不引入 lwIP；连接后的 byte face 可按需交给 Nexus
TLS／MQTT。TLS 的信任、熵、可信 UTC、arena 与其退出条件独立提供。

## 选择与运行顺序

```mermaid
flowchart LR
    U[已拥有的 UART] --> B[UARTByteStream 稳定副本与 inner request]
    B --> A[ATClient framing]
    A --> E[ESP-AT 选定模块状态]
    E --> T[复制 TCP byte face]
    T --> P[可选 TLS / MQTT / 产品协议]
```

1. 产品提供并启动精确 UART，建立模块和接收输入的已知静默边界。
2. 初始化 UARTByteStream 和 ESPAT，保存全部缓冲直到消费者和底层请求退出。
3. `configure` 发出 GMR，要求 AT core 报告 ESP32C3 标记，并匹配
   `Bin version:4.1.1.0(MINI-1)`。AT core 的数字版本与项目 Bin version 是不同事实。
4. 在同一有限 deadline 下设置 ATE0、SYSSTORE=0、CWMODE=1,0、CIPMUX=0、
   CIPMODE=0、CIPDINFO=0、CIPRECVTYPE=0。GMR 不匹配时停止模式配置。
5. 显式 `join` 选择 SSID／密码，等待实际 `WIFI GOT IP` 和 OK；再 `connect`
   一个 IPv4／域名 TCP 端点，等待 CONNECT 与 OK。
6. 既有 executor 每轮先推进 UARTByteStream，再以明确 `io_budget` 推进 ESPAT。
   每次 ESPAT service 至多执行该数量的实际 UART read/write，不建立隐藏等待。

GMR 匹配表示模块报告符合所选软件 profile；实际固件身份和烧录记录由产品维护。
命令使用 volatile 参数模式，wrapper 不保存网络凭据到产品存储。SSID 中的逗号、
引号与反斜杠按官方语法转义；join 完成后清理模块 command 缓冲。UART 和其他副本
仍按其各自生命周期与产品访问权限管理。

## 收发、容量与失败

| 项目 | 当前合同 |
| --- | --- |
| 发送 | 一次复制至多选定 TX 容量，最多 8192 B；输入在返回后可复用 |
| 实际发送 | `AT+CIPSEND` 的 OK 与无 CRLF prompt 后才逐步提交 payload |
| SEND OK | 表示 ESP 协议栈接纳，远端应用确认由上层协议提供 |
| 接收 | 单连接 active 模式，`+IPD,<len>:` 后按精确长度接收二进制 |
| 接收上限 | 官方 profile 单次至多 2920 B；还必须满足调用者 RX 剩余容量 |
| binary | 保留 CR、LF、NUL、`>`，支持 header 与 payload 同一次 read |
| 背压 | 单个 pending TX 返回 BUSY；RX 容量不足明确关闭准入 |
| EOF | 实际 CLOSED 后先读完已复制 RX，再返回 EOF |
| 失败 | malformed/overflow、loss、reset、timeout 或异常 URC 锁存第一错误 |

RX 使用显式有界环；发布的是复制字节，普通 read 不交出 DMA loan。长度错误或容量
不足不能跳过数据继续解释后续命令。取消等待和超时可能发生在命令／发送已经生效
之后；产品先退出上层消费者、停止新调用，按 UART 合同排空，再实际 reset/drain 到
已知边界，最后调用 `recover`。recover 只恢复软件状态，下次仍显式 configure。

一个 TCP 会话的 CLOSED 为终点，之后不直接开始新连接，避免把迟到 URC 绑定到新
会话。close 等待真实 CLOSED 与 OK，且要求既有 TX 与上层用户先退出。硬件电气、
射频、接入点、吞吐、功耗和实板串口均保留独立验证范围。

## 资源与验证

缓冲最小为 line 128 B、command 224 B、正容量 TX／RX。维护的 M4F short-enum
ABI 中模块状态为 192 B（包含 ATClient），另计所有调用者缓冲和 UARTByteStream。
为单次最大 active 数据准备至少 2920 B RX；较小配置是明确的容量限制。
未链接此组件的镜像不承担模块实例或缓冲。

`esp_at_test` 直接执行生产 C、真实 AT parser 和 request helper，外部 UART／模块
响应位于模型边界；其中一个完整双向会话同时执行真实 UARTByteStream。覆盖无
CRLF prompt、分片 header、binary 同包、溢出、迟到结果、timeout、close 和恢复。
模型不建立真实 ESP 固件互通或物理时序资格，HIL 为 `not_executed`。

依据为 [官方 v4.1.1.0 release](https://github.com/espressif/esp-at/releases/tag/v4.1.1.0)
及该 tag 的 [TCP/IP 命令](https://github.com/espressif/esp-at/blob/v4.1.1.0/docs/en/AT_Command_Set/TCP-IP_AT_Commands.rst)、
[基础命令](https://github.com/espressif/esp-at/blob/v4.1.1.0/docs/en/AT_Command_Set/Basic_AT_Commands.rst)
和 [Wi-Fi 命令](https://github.com/espressif/esp-at/blob/v4.1.1.0/docs/en/AT_Command_Set/Wi-Fi_AT_Commands.rst)。
tag 对应提交 `7c092f9aee793d6dbfae31e7585637baa14fb4ce`。模块固件不进入 Nexus 镜像或
默认依赖准备。
