日志框架
========

概述
----

日志框架为 Nexus 嵌入式平台提供统一的日志接口。支持多日志级别、多输出后端、
模块级过滤，以及同步和异步两种模式。

特性
----

- **多日志级别**: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- **多后端输出**: Console (stdout), UART, Memory (测试用)
- **模块过滤**: 支持通配符的模块级日志级别
- **可定制格式**: 时间戳、级别、模块、文件、行号、函数名
- **同步/异步模式**: 异步模式支持非阻塞日志写入
- **线程安全**: 多任务环境下安全使用
- **编译时优化**: 可在编译时移除低级别日志
- **资源可配置**: 支持静态分配，适用于资源受限系统

快速开始
--------

**基本使用:**

.. code-block:: c

    #define LOG_MODULE "app"
    #include "log/log.h"

    void app_init(void)
    {
        // 使用默认配置初始化
        log_init(NULL);

        // 使用便捷宏
        LOG_TRACE("详细跟踪信息");
        LOG_DEBUG("调试值: %d", 42);
        LOG_INFO("应用启动成功");
        LOG_WARN("资源使用率达到 80%%");
        LOG_ERROR("打开文件失败: %s", "config.txt");
        LOG_FATAL("严重系统故障");

        // 清理
        log_deinit();
    }

日志级别
--------

日志级别从最详细 (TRACE) 到最简略 (FATAL) 排序:

+------------------+-------+------------------+
| 级别             | 值    | 描述             |
+==================+=======+==================+
| LOG_LEVEL_TRACE  | 0     | 最详细跟踪信息   |
+------------------+-------+------------------+
| LOG_LEVEL_DEBUG  | 1     | 调试信息         |
+------------------+-------+------------------+
| LOG_LEVEL_INFO   | 2     | 一般信息         |
+------------------+-------+------------------+
| LOG_LEVEL_WARN   | 3     | 警告消息         |
+------------------+-------+------------------+
| LOG_LEVEL_ERROR  | 4     | 错误消息         |
+------------------+-------+------------------+
| LOG_LEVEL_FATAL  | 5     | 致命错误         |
+------------------+-------+------------------+
| LOG_LEVEL_NONE   | 6     | 禁用所有日志     |
+------------------+-------+------------------+

配置
----

**自定义配置:**

.. code-block:: c

    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,           // 过滤 TRACE
        .format = "[%T] [%L] [%M] %m",      // 自定义格式
        .async_mode = false,                // 同步模式
        .buffer_size = 0,                   // 同步模式不使用
        .max_msg_len = 256,                 // 最大消息长度
        .color_enabled = true               // 启用 ANSI 颜色
    };

    log_init(&config);

格式化 Token
------------

格式模式支持以下 Token:

+--------+------------------+---------------+
| Token  | 描述             | 示例          |
+========+==================+===============+
| ``%T`` | 毫秒时间戳       | ``12345678``  |
+--------+------------------+---------------+
| ``%t`` | HH:MM:SS 时间    | ``14:30:25``  |
+--------+------------------+---------------+
| ``%L`` | 完整级别名       | ``INFO``      |
+--------+------------------+---------------+
| ``%l`` | 短级别名         | ``I``         |
+--------+------------------+---------------+
| ``%M`` | 模块名           | ``app``       |
+--------+------------------+---------------+
| ``%F`` | 文件名           | ``main.c``    |
+--------+------------------+---------------+
| ``%f`` | 函数名           | ``app_init``  |
+--------+------------------+---------------+
| ``%n`` | 行号             | ``42``        |
+--------+------------------+---------------+
| ``%m`` | 消息内容         | ``Hello``     |
+--------+------------------+---------------+
| ``%c`` | ANSI 颜色码      | -             |
+--------+------------------+---------------+
| ``%C`` | ANSI 颜色重置    | -             |
+--------+------------------+---------------+
| ``%%`` | 字面百分号       | ``%``         |
+--------+------------------+---------------+

**默认格式:** ``[%T] [%L] [%M] %m``

后端
----

Console 后端
^^^^^^^^^^^^

输出到标准输出，适用于 Native 平台调试。

.. code-block:: c

    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);

    // 使用完毕后
    log_backend_unregister("console");
    log_backend_console_destroy(console);

UART 后端
^^^^^^^^^

输出到 UART 串口，适用于嵌入式目标平台。

.. code-block:: c

    // 先初始化 UART
    hal_uart_config_t uart_cfg = {
        .baudrate = 115200,
        .wordlen = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };
    hal_uart_init(HAL_UART_0, &uart_cfg);

    // 创建并注册 UART 后端
    log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
    log_backend_register(uart);

    // 使用完毕后
    log_backend_unregister("uart");
    log_backend_uart_destroy(uart);
    hal_uart_deinit(HAL_UART_0);

Memory 后端
^^^^^^^^^^^

输出到内存缓冲区，适用于测试和调试。

.. code-block:: c

    log_backend_t* memory = log_backend_memory_create(4096);
    log_backend_register(memory);

    // 读取缓冲区内容
    char buf[256];
    size_t len = log_backend_memory_read(memory, buf, sizeof(buf));

    // 清空缓冲区
    log_backend_memory_clear(memory);

    // 使用完毕后
    log_backend_unregister("memory");
    log_backend_memory_destroy(memory);

多后端
^^^^^^

消息会发送到所有已注册的后端:

.. code-block:: c

    log_init(NULL);

    // 注册多个后端
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);

    log_backend_t* memory = log_backend_memory_create(4096);
    log_backend_register(memory);

    // 消息发送到两个后端
    LOG_INFO("消息发送到 Console 和 Memory");

模块过滤
--------

为不同模块设置不同的日志级别:

.. code-block:: c

    log_init(NULL);
    log_set_level(LOG_LEVEL_INFO);  // 全局级别: INFO

    // 为 HAL 模块启用 DEBUG (通配符)
    log_module_set_level("hal.*", LOG_LEVEL_DEBUG);

    // 网络模块只显示 WARN 及以上
    log_module_set_level("network", LOG_LEVEL_WARN);

    // 获取模块有效级别
    log_level_t level = log_module_get_level("hal.gpio");  // 返回 DEBUG
    log_level_t level2 = log_module_get_level("app");      // 返回 INFO (全局)

    // 清除模块特定级别
    log_module_clear_level("network");

    // 清除所有模块特定级别
    log_module_clear_all();

异步模式
--------

异步模式支持非阻塞日志写入:

.. code-block:: c

    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,
        .format = "[%T] [%L] %m",
        .async_mode = true,                          // 启用异步
        .buffer_size = 4096,                         // 异步缓冲区大小
        .max_msg_len = 128,
        .async_queue_size = 32,                      // 队列深度
        .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST // 缓冲区满时策略
    };

    log_init(&config);

    // 日志写入立即返回
    for (int i = 0; i < 100; i++) {
        LOG_INFO("异步消息 %d", i);
    }

    // 等待所有消息处理完成
    log_async_flush();

    // 检查待处理消息数量
    size_t pending = log_async_pending();

**缓冲区满策略:**

+-----------------------------------+----------------------+
| 策略                              | 描述                 |
+===================================+======================+
| ``LOG_ASYNC_POLICY_DROP_OLDEST``  | 丢弃最旧消息         |
+-----------------------------------+----------------------+
| ``LOG_ASYNC_POLICY_DROP_NEWEST``  | 丢弃最新消息         |
+-----------------------------------+----------------------+
| ``LOG_ASYNC_POLICY_BLOCK``        | 阻塞等待空间         |
+-----------------------------------+----------------------+

后端级别过滤
------------

每个后端可以有独立的最小级别:

.. code-block:: c

    log_init(NULL);
    log_set_level(LOG_LEVEL_TRACE);  // 全局: 允许所有

    // Console 显示所有消息
    log_backend_t* console = log_backend_console_create();
    console->min_level = LOG_LEVEL_TRACE;
    log_backend_register(console);

    // UART 只显示 WARN 及以上
    log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
    uart->min_level = LOG_LEVEL_WARN;
    log_backend_register(uart);

    LOG_DEBUG("只输出到 Console");
    LOG_WARN("输出到 Console 和 UART");

运行时重配置
------------

日志设置可以在运行时修改:

.. code-block:: c

    // 修改日志级别
    log_set_level(LOG_LEVEL_DEBUG);

    // 修改格式
    log_set_format("[%l] %m");

    // 修改最大消息长度
    log_set_max_msg_len(64);

    // 启用/禁用后端
    log_backend_enable("uart", false);
    log_backend_enable("uart", true);

编译时配置
----------

**编译时级别过滤:**

在编译时移除低级别日志以减少代码体积:

.. code-block:: cmake

    # CMakeLists.txt
    add_definitions(-DLOG_COMPILE_LEVEL=LOG_LEVEL_INFO)

这会完全从二进制文件中移除 ``LOG_TRACE()`` 和 ``LOG_DEBUG()`` 调用。

**静态分配模式:**

禁用动态内存分配:

.. code-block:: cmake

    add_definitions(-DLOG_USE_STATIC_ALLOC=1)

**配置宏:**

+-------------------------------+--------------------+------------------+
| 宏                            | 默认值             | 描述             |
+===============================+====================+==================+
| ``LOG_DEFAULT_LEVEL``         | ``LOG_LEVEL_INFO`` | 默认日志级别     |
+-------------------------------+--------------------+------------------+
| ``LOG_MAX_MSG_LEN``           | ``128``            | 最大消息长度     |
+-------------------------------+--------------------+------------------+
| ``LOG_MAX_BACKENDS``          | ``4``              | 最大后端数量     |
+-------------------------------+--------------------+------------------+
| ``LOG_MAX_MODULE_FILTERS``    | ``16``             | 最大模块过滤器数 |
+-------------------------------+--------------------+------------------+
| ``LOG_COMPILE_LEVEL``         | ``LOG_LEVEL_TRACE``| 编译时级别       |
+-------------------------------+--------------------+------------------+
| ``LOG_USE_STATIC_ALLOC``      | ``0``              | 静态分配模式     |
+-------------------------------+--------------------+------------------+

自定义后端
----------

通过实现后端接口创建自定义后端:

.. code-block:: c

    static log_status_t my_backend_write(void* ctx, const char* msg, size_t len)
    {
        // 自定义输出逻辑 (文件、网络等)
        return LOG_OK;
    }

    static log_backend_t my_backend = {
        .name = "custom",
        .init = NULL,              // 可选
        .write = my_backend_write, // 必需
        .flush = NULL,             // 可选
        .deinit = NULL,            // 可选
        .ctx = NULL,               // 自定义上下文
        .min_level = LOG_LEVEL_INFO,
        .enabled = true
    };

    log_backend_register(&my_backend);

线程安全
--------

日志框架在多任务环境下是线程安全的:

- 使用 OSAL Mutex 保护共享状态
- 最小化锁持有时间
- 异步模式使用无锁队列

依赖
----

- **OSAL**: 操作系统抽象层 (Mutex, Queue, Task)
- **HAL**: 硬件抽象层 (UART)

API 参考
--------

完整 API 文档请参见 :doc:`../api/log`。
