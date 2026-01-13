/**
 * \file            log_example_cn.c
 * \brief           日志框架使用示例 (中文注释版)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         本文件演示 Nexus 日志框架的各种使用模式，
 *                  包括基本日志、配置、多后端、模块过滤和异步模式。
 */

/* 在包含 log.h 之前定义模块名 */
#define LOG_MODULE "example"

#include "hal/hal_uart.h"
#include "log/log.h"

/*---------------------------------------------------------------------------*/
/* 示例 1: 基本日志                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           基本日志示例
 * \details         展示如何初始化日志系统并使用日志宏
 */
void basic_logging_example(void) {
    /* 使用默认配置初始化 */
    log_init(NULL);

    /* 使用便捷宏记录不同级别的日志 */
    LOG_TRACE("详细跟踪信息");
    LOG_DEBUG("调试值: %d", 42);
    LOG_INFO("应用启动成功");
    LOG_WARN("资源使用率达到 80%%");
    LOG_ERROR("打开文件失败: %s", "config.txt");
    LOG_FATAL("严重系统故障");

    /* 清理资源 */
    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* 示例 2: 自定义配置                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           自定义配置示例
 * \details         展示如何使用自定义设置配置日志系统
 */
void custom_config_example(void) {
    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,      /* 过滤掉 TRACE 消息 */
        .format = "[%T] [%L] [%M] %m", /* 自定义格式模式 */
        .async_mode = false,           /* 同步模式 */
        .buffer_size = 0,              /* 同步模式不使用 */
        .max_msg_len = 256,            /* 最大消息长度 */
        .color_enabled = true          /* 启用 ANSI 颜色 */
    };

    log_init(&config);

    LOG_DEBUG("这条消息会被记录");
    LOG_TRACE("这条消息会被过滤掉");

    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* 示例 3: 多后端输出                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           多后端示例
 * \details         展示如何注册多个输出后端
 */
void multiple_backends_example(void) {
    log_init(NULL);

    /* 创建并注册 Console 后端 (stdout) */
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);

    /* 创建并注册 Memory 后端用于测试 */
    log_backend_t* memory = log_backend_memory_create(4096);
    log_backend_register(memory);

    /* 创建并注册 UART 后端 */
    hal_uart_config_t uart_cfg = {.baudrate = 115200,
                                  .wordlen = HAL_UART_WORDLEN_8,
                                  .stopbits = HAL_UART_STOPBITS_1,
                                  .parity = HAL_UART_PARITY_NONE,
                                  .flowctrl = HAL_UART_FLOWCTRL_NONE};
    hal_uart_init(HAL_UART_0, &uart_cfg);
    log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
    log_backend_register(uart);

    /* 日志消息会发送到所有后端 */
    LOG_INFO("消息发送到 Console、Memory 和 UART");

    /* 从 Memory 后端读取 */
    char buf[256];
    size_t len = log_backend_memory_read(memory, buf, sizeof(buf));
    (void)len; /* 抑制未使用警告 */

    /* 清理资源 */
    log_backend_unregister("console");
    log_backend_unregister("memory");
    log_backend_unregister("uart");
    log_backend_console_destroy(console);
    log_backend_memory_destroy(memory);
    log_backend_uart_destroy(uart);
    hal_uart_deinit(HAL_UART_0);
    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* 示例 4: 模块级过滤                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           模块级过滤示例
 * \details         展示如何为不同模块设置不同的日志级别
 */
void module_filtering_example(void) {
    log_init(NULL);
    log_set_level(LOG_LEVEL_INFO); /* 全局级别: INFO */

    /* 为 HAL 模块设置特定级别 */
    log_module_set_level("hal.*", LOG_LEVEL_DEBUG);

    /* 为单个模块设置特定级别 */
    log_module_set_level("network", LOG_LEVEL_WARN);

    /* 来自 "hal.gpio" 的消息会显示 DEBUG 及以上
     * 来自 "network" 的消息会显示 WARN 及以上
     * 来自其他模块的消息会显示 INFO 及以上 */

    /* 获取模块的有效级别 */
    log_level_t hal_level = log_module_get_level("hal.gpio");
    log_level_t net_level = log_module_get_level("network");
    log_level_t app_level = log_module_get_level("app"); /* 返回全局级别 */

    (void)hal_level;
    (void)net_level;
    (void)app_level;

    /* 清除模块特定级别 */
    log_module_clear_level("network");

    /* 清除所有模块特定级别 */
    log_module_clear_all();

    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* 示例 5: 异步日志                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           异步日志示例
 * \details         展示如何使用异步模式进行非阻塞日志记录
 */
void async_logging_example(void) {
    log_config_t config = {
        .level = LOG_LEVEL_DEBUG,
        .format = "[%T] [%L] %m",
        .async_mode = true,  /* 启用异步模式 */
        .buffer_size = 4096, /* 异步缓冲区大小 */
        .max_msg_len = 128,
        .color_enabled = false,
        .async_queue_size = 32,                      /* 队列深度 */
        .async_policy = LOG_ASYNC_POLICY_DROP_OLDEST /* 缓冲区满时策略 */
    };

    log_init(&config);

    /* 注册后端 */
    log_backend_t* console = log_backend_console_create();
    log_backend_register(console);

    /* 记录消息 (非阻塞，排队等待后台处理) */
    for (int i = 0; i < 100; i++) {
        LOG_INFO("异步消息 %d", i);
    }

    /* 检查待处理消息数量 */
    size_t pending = log_async_pending();
    (void)pending;

    /* 刷新所有待处理消息 (阻塞) */
    log_async_flush();

    /* 运行时更改策略 */
    log_async_set_policy(LOG_ASYNC_POLICY_BLOCK);

    /* 清理 (自动刷新待处理消息) */
    log_backend_unregister("console");
    log_backend_console_destroy(console);
    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* 示例 6: 运行时重配置                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           运行时重配置示例
 * \details         展示如何在运行时更改日志设置
 */
void runtime_reconfig_example(void) {
    log_init(NULL);

    /* 运行时更改日志级别 */
    log_set_level(LOG_LEVEL_DEBUG);
    LOG_DEBUG("现在可以看到调试消息");

    log_set_level(LOG_LEVEL_ERROR);
    LOG_DEBUG("这条调试消息被过滤");
    LOG_ERROR("只有错误及以上级别可见");

    /* 运行时更改格式模式 */
    log_set_format("[%l] %m"); /* 短级别格式 */
    LOG_INFO("使用短格式");

    log_set_format("[%T] [%L] [%M] [%F:%n] %m"); /* 完整格式 */
    LOG_INFO("使用完整格式");

    /* 运行时更改最大消息长度 */
    log_set_max_msg_len(64);
    LOG_INFO("如果这条很长的消息超过 64 个字符将被截断...");

    /* 重置为默认最大长度 */
    log_set_max_msg_len(0); /* 0 表示使用默认值 */

    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* 示例 7: 后端级别过滤                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           后端级别过滤示例
 * \details         展示如何为不同后端设置不同的日志级别
 */
void backend_filtering_example(void) {
    log_init(NULL);
    log_set_level(LOG_LEVEL_TRACE); /* 全局: 允许所有 */

    /* Console 后端: 显示所有消息 */
    log_backend_t* console = log_backend_console_create();
    console->min_level = LOG_LEVEL_TRACE;
    log_backend_register(console);

    /* UART 后端: 只显示警告及以上 */
    hal_uart_config_t uart_cfg = {.baudrate = 115200,
                                  .wordlen = HAL_UART_WORDLEN_8,
                                  .stopbits = HAL_UART_STOPBITS_1,
                                  .parity = HAL_UART_PARITY_NONE,
                                  .flowctrl = HAL_UART_FLOWCTRL_NONE};
    hal_uart_init(HAL_UART_0, &uart_cfg);
    log_backend_t* uart = log_backend_uart_create(HAL_UART_0);
    uart->min_level = LOG_LEVEL_WARN;
    log_backend_register(uart);

    /* 这条只输出到 Console */
    LOG_DEBUG("调试消息");

    /* 这条输出到 Console 和 UART */
    LOG_WARN("警告消息");

    /* 运行时启用/禁用后端 */
    log_backend_enable("uart", false); /* 禁用 UART */
    LOG_ERROR("错误只输出到 Console");

    log_backend_enable("uart", true); /* 重新启用 UART */
    LOG_ERROR("错误输出到两者");

    /* 清理资源 */
    log_backend_unregister("console");
    log_backend_unregister("uart");
    log_backend_console_destroy(console);
    log_backend_uart_destroy(uart);
    hal_uart_deinit(HAL_UART_0);
    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* 示例 8: 编译时配置                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           编译时配置示例
 * \details         展示如何使用编译时选项减少代码体积
 *
 * 要在编译时禁用 TRACE 和 DEBUG，在构建时添加:
 * \code
 * -DLOG_COMPILE_LEVEL=LOG_LEVEL_INFO
 * \endcode
 *
 * 这会完全从二进制文件中移除 LOG_TRACE 和 LOG_DEBUG 调用。
 *
 * 要使用静态分配 (无 malloc/free):
 * \code
 * -DLOG_USE_STATIC_ALLOC=1
 * \endcode
 *
 * 要自定义缓冲区大小:
 * \code
 * -DLOG_MAX_MSG_LEN=64
 * -DLOG_MAX_BACKENDS=2
 * -DLOG_MAX_MODULE_FILTERS=8
 * \endcode
 */
void compile_time_config_example(void) {
    /* 当 LOG_COMPILE_LEVEL=LOG_LEVEL_INFO 时:
     * - LOG_TRACE() 变成 ((void)0) - 不生成代码
     * - LOG_DEBUG() 变成 ((void)0) - 不生成代码
     * - LOG_INFO() 及以上正常工作
     */

    log_init(NULL);

    /* 这些可能根据 LOG_COMPILE_LEVEL 被编译移除 */
    LOG_TRACE("可能被编译移除");
    LOG_DEBUG("可能被编译移除");

    /* 这些总是被编译 (除非 LOG_COMPILE_LEVEL > 它们的级别) */
    LOG_INFO("总是被编译");
    LOG_WARN("总是被编译");
    LOG_ERROR("总是被编译");
    LOG_FATAL("总是被编译");

    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* 示例 9: 自定义后端                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           自定义后端写入函数
 */
static log_status_t custom_backend_write(void* ctx, const char* msg,
                                         size_t len) {
    /* 自定义输出逻辑，例如写入文件、发送到网络等 */
    (void)ctx;
    (void)msg;
    (void)len;
    return LOG_OK;
}

/**
 * \brief           自定义后端示例
 * \details         展示如何创建和注册自定义后端
 */
void custom_backend_example(void) {
    log_init(NULL);

    /* 创建自定义后端结构 */
    static log_backend_t custom_backend = {
        .name = "custom",
        .init = NULL,                  /* 可选 */
        .write = custom_backend_write, /* 必需 */
        .flush = NULL,                 /* 可选 */
        .deinit = NULL,                /* 可选 */
        .ctx = NULL,                   /* 自定义上下文 */
        .min_level = LOG_LEVEL_INFO,   /* 最小级别 */
        .enabled = true                /* 启用状态 */
    };

    /* 注册自定义后端 */
    log_backend_register(&custom_backend);

    /* 使用日志 */
    LOG_INFO("消息发送到自定义后端");

    /* 清理 */
    log_backend_unregister("custom");
    log_deinit();
}

/*---------------------------------------------------------------------------*/
/* 主函数 - 运行所有示例                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           主函数
 * \details         依次运行所有示例
 */
int main(void) {
    basic_logging_example();
    custom_config_example();
    /* multiple_backends_example(); // 需要 UART 硬件 */
    module_filtering_example();
    async_logging_example();
    runtime_reconfig_example();
    /* backend_filtering_example(); // 需要 UART 硬件 */
    compile_time_config_example();
    custom_backend_example();

    return 0;
}
