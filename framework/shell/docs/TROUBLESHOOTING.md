# Shell Framework 故障排查指南

本文档提供 Nexus Shell Framework 常见问题的诊断和解决方案。

## 目录

1. [初始化问题](#初始化问题)
2. [输入输出问题](#输入输出问题)
3. [命令执行问题](#命令执行问题)
4. [行编辑问题](#行编辑问题)
5. [历史功能问题](#历史功能问题)
6. [自动补全问题](#自动补全问题)
7. [后端问题](#后端问题)
8. [性能问题](#性能问题)
9. [内存问题](#内存问题)
10. [调试技巧](#调试技巧)
11. [常见错误码](#常见错误码)
12. [获取帮助](#获取帮助)

## 初始化问题

### 问题 1.1：Shell 初始化失败

**症状**：
```c
shell_status_t status = shell_init(&config);
/* status == SHELL_ERROR_INVALID_PARAM */
```

**可能原因**：
1. 配置参数无效
2. 配置指针为 NULL
3. 缓冲区大小超出范围
4. 历史深度超出范围

**诊断步骤**：

```c
/* 1. 检查配置参数 */
shell_config_t config = {
    .prompt = "nexus> ",
    .cmd_buffer_size = 128,  /* 必须在 64-256 之间 */
    .history_depth = 16,     /* 必须在 4-32 之间 */
    .max_commands = 32
};

/* 2. 验证参数范围 */
if (config.cmd_buffer_size < 64 || config.cmd_buffer_size > 256) {
    printf("Invalid cmd_buffer_size: %d\n", config.cmd_buffer_size);
}

if (config.history_depth < 4 || config.history_depth > 32) {
    printf("Invalid history_depth: %d\n", config.history_depth);
}

/* 3. 检查提示符长度 */
if (strlen(config.prompt) > 16) {
    printf("Prompt too long: %zu\n", strlen(config.prompt));
}
```

**解决方案**：

```c
/* 使用默认配置 */
shell_config_t config = SHELL_CONFIG_DEFAULT;
shell_status_t status = shell_init(&config);

if (status != SHELL_OK) {
    printf("Init failed: %s\n", shell_get_error_message(status));
}
```

### 问题 1.2：内存分配失败

**症状**：
```c
shell_init(&config);
/* 返回 SHELL_ERROR_NO_MEMORY */
```

**可能原因**：
1. 堆内存不足
2. 内存碎片
3. 配置的缓冲区太大

**诊断步骤**：

```c
/* 1. 检查可用堆内存 */
size_t free_heap = get_free_heap_size();
printf("Free heap: %zu bytes\n", free_heap);

/* 2. 计算所需内存 */
size_t required = 0;
required += config.cmd_buffer_size;           /* 命令缓冲区 */
required += config.cmd_buffer_size;           /* 保存输入缓冲区 */
required += config.history_depth * config.cmd_buffer_size;  /* 历史存储 */
required += config.history_depth * sizeof(char*);           /* 历史指针 */
required += sizeof(shell_context_t);          /* Shell 上下文 */

printf("Required memory: %zu bytes\n", required);

if (free_heap < required) {
    printf("Insufficient memory!\n");
}
```

**解决方案**：

```c
/* 方案 1：减小缓冲区大小 */
shell_config_t config = {
    .prompt = "$ ",
    .cmd_buffer_size = 64,   /* 减小到 64 */
    .history_depth = 4,      /* 减小到 4 */
    .max_commands = 16
};

/* 方案 2：增加堆大小（修改链接器脚本） */
/* 在链接器脚本中增加 _Min_Heap_Size */

/* 方案 3：使用静态分配（未来功能） */
#define SHELL_USE_STATIC_ALLOC 1
```

### 问题 1.3：重复初始化

**症状**：
```c
shell_init(&config);  /* 第一次 */
shell_init(&config);  /* 第二次，返回 SHELL_ERROR_ALREADY_INIT */
```

**解决方案**：

```c
/* 检查是否已初始化 */
if (!shell_is_initialized()) {
    shell_init(&config);
} else {
    printf("Shell already initialized\n");
}

/* 或者先反初始化 */
if (shell_is_initialized()) {
    shell_deinit();
}
shell_init(&config);
```


## 输入输出问题

### 问题 2.1：无输入响应

**症状**：
- 输入字符没有回显
- 命令无法执行
- Shell 完全无响应

**可能原因**：
1. Shell 未初始化
2. 后端未配置
3. UART 未初始化
4. 未调用 `shell_process()`
5. 后端读取函数有问题

**诊断步骤**：

```c
/* 1. 检查初始化状态 */
if (!shell_is_initialized()) {
    printf("ERROR: Shell not initialized\n");
}

/* 2. 检查后端 */
const shell_backend_t* backend = shell_get_backend();
if (backend == NULL) {
    printf("ERROR: No backend configured\n");
}

/* 3. 测试后端读取 */
char c;
int n = backend->read(&c, 1);
printf("Backend read returned: %d\n", n);

/* 4. 检查 UART 状态 */
if (hal_uart_available(HAL_UART_0) > 0) {
    printf("UART has data\n");
} else {
    printf("UART no data\n");
}

/* 5. 确认 shell_process 被调用 */
static int process_count = 0;
void main_loop(void) {
    shell_process();
    process_count++;
    if (process_count % 1000 == 0) {
        printf("shell_process called %d times\n", process_count);
    }
}
```

**解决方案**：

```c
/* 完整的初始化流程 */
void shell_setup(void) {
    /* 1. 初始化 UART */
    hal_uart_config_t uart_cfg = {
        .baudrate = 115200,
        .wordlen = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity = HAL_UART_PARITY_NONE
    };
    
    hal_status_t hal_status = hal_uart_init(HAL_UART_0, &uart_cfg);
    if (hal_status != HAL_OK) {
        printf("UART init failed\n");
        return;
    }
    
    /* 2. 初始化 Shell */
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_status_t status = shell_init(&config);
    if (status != SHELL_OK) {
        printf("Shell init failed: %d\n", status);
        return;
    }
    
    /* 3. 设置后端 */
    shell_set_backend(&shell_uart_backend);
    
    /* 4. 注册命令 */
    shell_register_builtin_commands();
    
    /* 5. 打印提示符 */
    shell_print_prompt();
}

/* 主循环 */
int main(void) {
    shell_setup();
    
    while (1) {
        shell_process();  /* 必须周期性调用 */
        
        /* 其他任务 */
    }
}
```

### 问题 2.2：输出乱码

**症状**：
- Shell 输出显示乱码
- 字符显示不正确
- 特殊字符显示异常

**可能原因**：
1. 波特率不匹配
2. 数据位/停止位/校验位配置错误
3. 终端编码设置错误
4. UART 时钟配置错误

**诊断步骤**：

```c
/* 1. 验证 UART 配置 */
void verify_uart_config(void) {
    printf("UART Configuration:\n");
    printf("  Baudrate: %d\n", get_uart_baudrate());
    printf("  Data bits: %d\n", get_uart_databits());
    printf("  Stop bits: %d\n", get_uart_stopbits());
    printf("  Parity: %d\n", get_uart_parity());
}

/* 2. 发送测试字符 */
void test_uart_output(void) {
    const char* test = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n";
    hal_uart_write(HAL_UART_0, (const uint8_t*)test, strlen(test));
}

/* 3. 测试不同波特率 */
void test_baudrates(void) {
    uint32_t rates[] = {9600, 19200, 38400, 57600, 115200, 230400};
    
    for (int i = 0; i < 6; i++) {
        hal_uart_set_baudrate(HAL_UART_0, rates[i]);
        hal_delay_ms(100);
        
        const char* msg = "Test\r\n";
        hal_uart_write(HAL_UART_0, (const uint8_t*)msg, strlen(msg));
        hal_delay_ms(500);
    }
}
```

**解决方案**：

```c
/* 1. 确保配置匹配 */
/* 设备端 */
hal_uart_config_t uart_cfg = {
    .baudrate = 115200,
    .wordlen = HAL_UART_WORDLEN_8,
    .stopbits = HAL_UART_STOPBITS_1,
    .parity = HAL_UART_PARITY_NONE
};

/* 终端端（minicom 示例） */
/* 波特率: 115200 */
/* 数据位: 8 */
/* 停止位: 1 */
/* 校验: None */
/* 流控: None */

/* 2. 检查时钟配置 */
/* 确保 UART 时钟源正确 */
/* 确保时钟分频正确 */

/* 3. 使用示波器验证 */
/* 测量实际波特率 */
/* 检查信号质量 */
```

### 问题 2.3：输出不完整

**症状**：
- 部分字符丢失
- 输出被截断
- 长消息显示不完整

**可能原因**：
1. UART 发送缓冲区满
2. 发送速度太快
3. 流控未启用
4. 终端接收缓冲区溢出

**诊断步骤**：

```c
/* 1. 测试发送速度 */
void test_send_speed(void) {
    const char* msg = "0123456789ABCDEF";
    
    for (int i = 0; i < 100; i++) {
        shell_printf("%s\r\n", msg);
        /* 不加延迟，看是否丢失 */
    }
}

/* 2. 检查发送缓冲区 */
int get_uart_tx_buffer_free(void) {
    /* 返回发送缓冲区剩余空间 */
    return uart_tx_buffer_free();
}

/* 3. 监控发送状态 */
void monitor_uart_tx(void) {
    while (1) {
        int free = get_uart_tx_buffer_free();
        printf("TX buffer free: %d\r\n", free);
        hal_delay_ms(100);
    }
}
```

**解决方案**：

```c
/* 方案 1：添加延迟 */
void shell_printf_slow(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    /* 分块发送 */
    for (int i = 0; i < len; i += 16) {
        int chunk = (len - i > 16) ? 16 : (len - i);
        shell_puts(&buffer[i]);
        hal_delay_ms(1);  /* 添加小延迟 */
    }
}

/* 方案 2：等待发送完成 */
void uart_write_blocking(const uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        /* 等待发送缓冲区有空间 */
        while (uart_tx_buffer_full()) {
            /* 等待 */
        }
        uart_send_byte(buf[i]);
    }
}

/* 方案 3：启用硬件流控 */
hal_uart_config_t uart_cfg = {
    .baudrate = 115200,
    .wordlen = HAL_UART_WORDLEN_8,
    .stopbits = HAL_UART_STOPBITS_1,
    .parity = HAL_UART_PARITY_NONE,
    .flowctrl = HAL_UART_FLOWCTRL_RTS_CTS  /* 启用流控 */
};
```

### 问题 2.4：回显重复

**症状**：
- 输入的字符显示两次
- 命令执行后输出重复

**可能原因**：
1. 终端启用了本地回显
2. Shell 和终端都在回显
3. UART 配置了回环模式

**解决方案**：

```c
/* 1. 禁用终端本地回显 */
/* minicom: Ctrl+A Z -> O -> Serial port setup -> F (Hardware Flow Control) */
/* PuTTY: Terminal -> Local echo: Force off */

/* 2. 检查 UART 配置 */
/* 确保未启用回环模式 */

/* 3. 使用原始模式（Linux） */
struct termios raw;
tcgetattr(STDIN_FILENO, &raw);
raw.c_lflag &= ~(ECHO | ICANON);  /* 禁用回显 */
tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
```

## 命令执行问题

### 问题 3.1：命令未找到

**症状**：
```
nexus> mycommand
Unknown command: mycommand
```

**可能原因**：
1. 命令未注册
2. 命令名拼写错误
3. 注册失败但未检查

**诊断步骤**：

```c
/* 1. 检查命令是否注册 */
const shell_command_t* cmd = shell_find_command("mycommand");
if (cmd == NULL) {
    printf("Command 'mycommand' not registered\n");
} else {
    printf("Command found: %s\n", cmd->name);
}

/* 2. 列出所有命令 */
void list_all_commands(void) {
    uint8_t count = shell_get_command_count();
    printf("Total commands: %d\n", count);
    
    /* 遍历所有命令 */
    for (uint8_t i = 0; i < count; i++) {
        const shell_command_t* cmd = shell_get_command_by_index(i);
        if (cmd != NULL) {
            printf("  %s - %s\n", cmd->name, cmd->help);
        }
    }
}

/* 3. 检查注册返回值 */
shell_status_t status = shell_register_command(&my_cmd);
if (status != SHELL_OK) {
    printf("Failed to register command: %s\n", 
           shell_get_error_message(status));
}
```

**解决方案**：

```c
/* 确保命令正确注册 */
static int cmd_mycommand(int argc, char* argv[]) {
    shell_printf("My command executed\r\n");
    return 0;
}

static const shell_command_t my_cmd = {
    .name = "mycommand",  /* 确保名称正确 */
    .handler = cmd_mycommand,
    .help = "My custom command",
    .usage = "mycommand"
};

void register_my_commands(void) {
    shell_status_t status = shell_register_command(&my_cmd);
    if (status != SHELL_OK) {
        printf("Registration failed: %d\n", status);
    } else {
        printf("Command registered successfully\n");
    }
}
```

### 问题 3.2：命令执行失败

**症状**：
```
nexus> mycommand
Error: command returned -1
```

**可能原因**：
1. 命令处理函数返回错误
2. 参数不正确
3. 资源不可用
4. 权限不足

**诊断步骤**：

```c
/* 添加详细的错误信息 */
static int cmd_mycommand(int argc, char* argv[]) {
    /* 检查参数 */
    if (argc < 2) {
        shell_printf("Error: missing argument\r\n");
        shell_printf("Usage: mycommand <arg>\r\n");
        return -1;
    }
    
    /* 执行操作 */
    int result = do_something(argv[1]);
    if (result != 0) {
        shell_printf("Error: operation failed with code %d\r\n", result);
        return -1;
    }
    
    shell_printf("Success\r\n");
    return 0;
}
```

### 问题 3.3：参数解析错误

**症状**：
- 参数数量不正确
- 参数内容错误
- 引号处理不正确

**诊断步骤**：

```c
/* 打印所有参数 */
static int cmd_debug_args(int argc, char* argv[]) {
    shell_printf("argc = %d\r\n", argc);
    
    for (int i = 0; i < argc; i++) {
        shell_printf("argv[%d] = \"%s\"\r\n", i, argv[i]);
    }
    
    return 0;
}

/* 测试不同的输入 */
/* nexus> debug_args arg1 arg2 arg3 */
/* nexus> debug_args "hello world" */
/* nexus> debug_args 'single quotes' */
/* nexus> debug_args "escaped \"quotes\"" */
```

**解决方案**：

```c
/* 正确处理参数 */
static int cmd_example(int argc, char* argv[]) {
    /* 1. 验证参数数量 */
    if (argc != 3) {
        shell_printf("Usage: example <arg1> <arg2>\r\n");
        return -1;
    }
    
    /* 2. 验证参数内容 */
    if (strlen(argv[1]) == 0) {
        shell_printf("Error: arg1 cannot be empty\r\n");
        return -1;
    }
    
    /* 3. 解析数值参数 */
    char* endptr;
    long value = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0') {
        shell_printf("Error: arg2 must be a number\r\n");
        return -1;
    }
    
    /* 4. 使用参数 */
    shell_printf("arg1=%s, arg2=%ld\r\n", argv[1], value);
    
    return 0;
}
```

## 行编辑问题

### 问题 4.1：光标位置错误

**症状**：
- 光标显示位置与实际位置不符
- 插入字符位置错误
- 删除字符位置错误

**可能原因**：
1. 转义序列处理错误
2. 显示刷新不正确
3. 终端不支持光标控制

**诊断步骤**：

```c
/* 测试光标移动 */
void test_cursor_movement(void) {
    /* 输入 "hello" */
    shell_mock_backend_set_input("hello");
    for (int i = 0; i < 5; i++) {
        shell_process();
    }
    
    /* 按 Home 键 */
    shell_mock_backend_set_input("\033[H");
    for (int i = 0; i < 3; i++) {
        shell_process();
    }
    
    /* 插入 "x" */
    shell_mock_backend_set_input("x");
    shell_process();
    
    /* 结果应该是 "xhello" */
}
```

**解决方案**：

```c
/* 确保终端支持 ANSI 转义序列 */
/* 测试终端：按左箭头应该看到 ESC[D */

/* 如果终端不支持，使用简化模式 */
#define SHELL_SIMPLE_MODE 1  /* 禁用光标控制 */
```

### 问题 4.2：删除功能异常

**症状**：
- Backspace 不工作
- Delete 键不工作
- 删除后显示错误

**诊断步骤**：

```c
/* 测试删除功能 */
void test_delete(void) {
    /* 输入 "hello" */
    const char* input = "hello\b\b";  /* 删除两个字符 */
    
    shell_mock_backend_set_input(input);
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    /* 结果应该是 "hel" */
}
```

**解决方案**：

```c
/* 检查 Backspace 键码 */
/* 不同终端可能发送不同的键码：*/
/* - 0x08 (BS) */
/* - 0x7F (DEL) */
/* - 0x1B 0x5B 0x33 0x7E (ESC[3~) */

/* Shell 应该同时支持这些键码 */
```

## 历史功能问题

### 问题 5.1：历史记录丢失

**症状**：
- 按上箭头没有反应
- 历史记录为空
- 执行的命令未保存

**可能原因**：
1. 历史深度设置为 0
2. 历史功能被禁用
3. 命令未添加到历史

**诊断步骤**：

```c
/* 检查历史配置 */
void check_history_config(void) {
    history_manager_t* hist = shell_get_history_manager();
    if (hist == NULL) {
        printf("History manager not available\n");
        return;
    }
    
    uint8_t count = history_get_count(hist);
    uint8_t capacity = history_get_capacity(hist);
    
    printf("History count: %d\n", count);
    printf("History capacity: %d\n", capacity);
    
    if (capacity == 0) {
        printf("ERROR: History depth is 0\n");
    }
}

/* 手动添加历史 */
void test_history_add(void) {
    history_manager_t* hist = shell_get_history_manager();
    if (hist != NULL) {
        history_add(hist, "test command");
        
        uint8_t count = history_get_count(hist);
        printf("History count after add: %d\n", count);
    }
}
```

**解决方案**：

```c
/* 确保历史深度 > 0 */
shell_config_t config = {
    .prompt = "nexus> ",
    .cmd_buffer_size = 128,
    .history_depth = 16,  /* 必须 > 0 */
    .max_commands = 32
};
```

### 问题 5.2：历史浏览异常

**症状**：
- 按上下箭头显示错误的命令
- 历史顺序混乱
- 无法回到当前输入

**诊断步骤**：

```c
/* 测试历史浏览 */
void test_history_browse(void) {
    /* 执行几条命令 */
    execute_command("cmd1");
    execute_command("cmd2");
    execute_command("cmd3");
    
    /* 按上箭头 */
    const char* cmd = history_get_prev(hist);
    printf("Prev: %s\n", cmd);  /* 应该是 cmd3 */
    
    cmd = history_get_prev(hist);
    printf("Prev: %s\n", cmd);  /* 应该是 cmd2 */
    
    cmd = history_get_next(hist);
    printf("Next: %s\n", cmd);  /* 应该是 cmd3 */
}
```


## 自动补全问题

### 问题 6.1：Tab 键无响应

**症状**：
- 按 Tab 键没有任何反应
- 自动补全不工作

**可能原因**：
1. Tab 键被终端拦截
2. 没有匹配的命令
3. 输入为空
4. 自动补全功能被禁用

**诊断步骤**：

```c
/* 1. 测试 Tab 键 */
void test_tab_key(void) {
    /* 在终端按 Tab，应该看到 0x09 */
    char c;
    if (backend->read(&c, 1) > 0) {
        printf("Received: 0x%02X\n", (uint8_t)c);
    }
}

/* 2. 测试补全功能 */
void test_autocomplete(void) {
    /* 注册命令 */
    shell_register_command(&help_cmd);
    shell_register_command(&hello_cmd);
    
    /* 输入 "he" 然后按 Tab */
    const char* input = "he\t";
    shell_mock_backend_set_input(input);
    
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    /* 应该显示 help 和 hello */
}
```

**解决方案**：

```c
/* 确保有注册的命令 */
shell_register_builtin_commands();

/* 确保输入不为空 */
/* 输入至少一个字符后再按 Tab */

/* 检查终端设置 */
/* 确保 Tab 键发送 0x09 而不是被终端处理 */
```

### 问题 6.2：补全结果错误

**症状**：
- 补全显示不相关的命令
- 补全结果不完整
- 公共前缀计算错误

**诊断步骤**：

```c
/* 测试补全逻辑 */
void test_completion_logic(void) {
    completion_result_t result;
    
    /* 测试单个匹配 */
    autocomplete_process("hel", 3, 3, &result);
    printf("Match count: %d\n", result.match_count);
    if (result.match_count == 1) {
        printf("Match: %s\n", result.matches[0]);
    }
    
    /* 测试多个匹配 */
    autocomplete_process("h", 1, 1, &result);
    printf("Match count: %d\n", result.match_count);
    for (int i = 0; i < result.match_count; i++) {
        printf("  %s\n", result.matches[i]);
    }
    
    /* 测试公共前缀 */
    char prefix[32];
    int len = autocomplete_get_common_prefix(&result, prefix, sizeof(prefix));
    printf("Common prefix: %s (len=%d)\n", prefix, len);
}
```

## 后端问题

### 问题 7.1：UART 通信失败

**症状**：
- 无法发送或接收数据
- UART 初始化失败
- 数据传输错误

**诊断步骤**：

```c
/* 1. 测试 UART 硬件 */
void test_uart_hardware(void) {
    /* 发送测试字符 */
    const char* test = "UART Test\r\n";
    int sent = hal_uart_write(HAL_UART_0, (const uint8_t*)test, strlen(test));
    printf("Sent %d bytes\n", sent);
    
    /* 接收测试 */
    uint8_t buf[16];
    int received = hal_uart_read(HAL_UART_0, buf, sizeof(buf));
    printf("Received %d bytes\n", received);
}

/* 2. 检查 UART 配置 */
void check_uart_config(void) {
    /* 检查引脚配置 */
    /* 检查时钟使能 */
    /* 检查中断配置 */
}

/* 3. 使用示波器 */
/* 测量 TX 和 RX 信号 */
/* 验证波特率 */
/* 检查信号电平 */
```

**解决方案**：

```c
/* 完整的 UART 初始化 */
hal_status_t uart_init_complete(void) {
    /* 1. 使能时钟 */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    /* 2. 配置 GPIO */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* 3. 配置 UART */
    UART_HandleTypeDef huart1;
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        return HAL_ERROR;
    }
    
    return HAL_OK;
}
```

### 问题 7.2：数据丢失

**症状**：
- 部分字符丢失
- 命令不完整
- 接收缓冲区溢出

**可能原因**：
1. 接收缓冲区太小
2. 处理速度太慢
3. 未使用中断接收
4. 流控未启用

**解决方案**：

```c
/* 方案 1：使用中断接收 */
static uint8_t rx_buffer[256];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

void USART1_IRQHandler(void) {
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        uint8_t data = (uint8_t)(huart1.Instance->DR & 0xFF);
        
        uint16_t next_head = (rx_head + 1) % sizeof(rx_buffer);
        if (next_head != rx_tail) {
            rx_buffer[rx_head] = data;
            rx_head = next_head;
        }
    }
}

int uart_backend_read(char* buf, size_t len) {
    if (rx_head == rx_tail) {
        return 0;
    }
    
    *buf = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % sizeof(rx_buffer);
    
    return 1;
}

/* 方案 2：使用 DMA */
void uart_dma_init(void) {
    HAL_UART_Receive_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
}

/* 方案 3：增加处理频率 */
void high_freq_task(void) {
    /* 每 1ms 调用一次 */
    shell_process();
}
```

## 性能问题

### 问题 8.1：响应慢

**症状**：
- 命令执行延迟高
- 字符回显慢
- 整体响应迟钝

**可能原因**：
1. shell_process() 调用频率太低
2. 命令处理函数有阻塞操作
3. UART 波特率太低
4. CPU 负载过高

**诊断步骤**：

```c
/* 1. 测量响应时间 */
void measure_response_time(void) {
    uint32_t start = get_tick_ms();
    
    /* 输入命令 */
    const char* input = "help\r";
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    uint32_t end = get_tick_ms();
    printf("Response time: %lu ms\n", end - start);
}

/* 2. 分析 CPU 使用率 */
void analyze_cpu_usage(void) {
    uint32_t idle_count = 0;
    uint32_t total_count = 0;
    
    for (int i = 0; i < 10000; i++) {
        uint32_t start = get_tick_us();
        shell_process();
        uint32_t elapsed = get_tick_us() - start;
        
        total_count++;
        if (elapsed < 10) {
            idle_count++;
        }
    }
    
    printf("CPU idle: %lu%%\n", idle_count * 100 / total_count);
}
```

**解决方案**：

```c
/* 1. 增加调用频率 */
void fast_main_loop(void) {
    while (1) {
        shell_process();  /* 尽可能频繁调用 */
        /* 不要添加延迟 */
    }
}

/* 2. 避免阻塞操作 */
static int cmd_no_blocking(int argc, char* argv[]) {
    /* 不要这样做 */
    /* hal_delay_ms(1000); */
    
    /* 应该使用异步方式 */
    start_async_operation();
    shell_printf("Operation started\r\n");
    return 0;
}

/* 3. 提高波特率 */
uart_cfg.baudrate = 921600;  /* 提高到 921600 */

/* 4. 使用 RTOS 优先级 */
xTaskCreate(shell_task, "shell", 1024, NULL, 
            configMAX_PRIORITIES - 1,  /* 高优先级 */
            NULL);
```

### 问题 8.2：内存占用高

**症状**：
- 内存使用超出预期
- 堆内存不足
- 栈溢出

**诊断步骤**：

```c
/* 1. 测量内存使用 */
void measure_memory_usage(void) {
    size_t before = get_free_heap_size();
    
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    size_t after = get_free_heap_size();
    size_t used = before - after;
    
    printf("Memory used: %zu bytes\n", used);
    printf("Free heap: %zu bytes\n", after);
}

/* 2. 检查栈使用 */
void check_stack_usage(void) {
    uint32_t stack_used = get_stack_usage();
    uint32_t stack_size = get_stack_size();
    
    printf("Stack used: %lu / %lu bytes\n", stack_used, stack_size);
    printf("Stack usage: %lu%%\n", stack_used * 100 / stack_size);
}
```

**解决方案**：

```c
/* 1. 减小配置 */
shell_config_t config = {
    .prompt = "$ ",
    .cmd_buffer_size = 64,
    .history_depth = 4,
    .max_commands = 16
};

/* 2. 增加堆大小 */
/* 在链接器脚本中修改 */
_Min_Heap_Size = 0x2000;  /* 8KB */

/* 3. 增加栈大小 */
_Min_Stack_Size = 0x1000;  /* 4KB */
```

## 内存问题

### 问题 9.1：内存泄漏

**症状**：
- 可用内存持续减少
- 长时间运行后崩溃
- 内存分配失败

**诊断步骤**：

```c
/* 1. 监控内存 */
void monitor_memory(void) {
    while (1) {
        size_t free = get_free_heap_size();
        printf("Free heap: %zu bytes\r\n", free);
        hal_delay_ms(1000);
    }
}

/* 2. 测试内存泄漏 */
void test_memory_leak(void) {
    size_t initial = get_free_heap_size();
    
    /* 执行多次初始化/反初始化 */
    for (int i = 0; i < 100; i++) {
        shell_config_t config = SHELL_CONFIG_DEFAULT;
        shell_init(&config);
        shell_deinit();
    }
    
    size_t final = get_free_heap_size();
    
    if (initial != final) {
        printf("Memory leak detected: %zu bytes\n", initial - final);
    }
}
```

**解决方案**：

```c
/* 确保正确反初始化 */
void cleanup(void) {
    /* 反初始化 Shell */
    shell_deinit();
    
    /* 反初始化 UART */
    hal_uart_deinit(HAL_UART_0);
    
    /* 释放其他资源 */
}
```

## 调试技巧

### 技巧 1：使用 Mock 后端

```c
/* 单元测试 */
void test_command(void) {
    /* 设置 Mock 后端 */
    const char* input = "help\r";
    char output[512] = {0};
    
    shell_mock_backend_set_input(input);
    shell_mock_backend_set_output(output, sizeof(output));
    shell_set_backend(&shell_mock_backend);
    
    /* 处理输入 */
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    /* 检查输出 */
    printf("Output:\n%s\n", output);
}
```

### 技巧 2：添加调试输出

```c
/* 启用调试模式 */
#define SHELL_DEBUG 1

#ifdef SHELL_DEBUG
#define SHELL_DEBUG_PRINTF(fmt, ...) printf("[SHELL] " fmt, ##__VA_ARGS__)
#else
#define SHELL_DEBUG_PRINTF(fmt, ...)
#endif

/* 在关键位置添加调试输出 */
shell_status_t shell_process(void) {
    SHELL_DEBUG_PRINTF("shell_process called\n");
    
    /* ... */
}
```

### 技巧 3：使用断点调试

```c
/* 在关键函数设置断点 */
static void execute_command_line(void) {
    __asm("nop");  /* 设置断点的位置 */
    
    /* 命令执行逻辑 */
}
```

### 技巧 4：日志记录

```c
/* 记录所有命令 */
static int cmd_wrapper(int argc, char* argv[]) {
    /* 记录命令 */
    LOG_INFO("Shell", "Executing: %s", argv[0]);
    
    /* 执行命令 */
    int ret = original_handler(argc, argv);
    
    /* 记录结果 */
    LOG_INFO("Shell", "Result: %d", ret);
    
    return ret;
}
```

## 常见错误码

| 错误码 | 名称 | 描述 | 解决方案 |
|--------|------|------|----------|
| 0 | SHELL_OK | 成功 | - |
| 1 | SHELL_ERROR | 通用错误 | 检查具体错误信息 |
| 2 | SHELL_ERROR_INVALID_PARAM | 无效参数 | 检查配置参数 |
| 3 | SHELL_ERROR_NOT_INIT | 未初始化 | 先调用 shell_init() |
| 4 | SHELL_ERROR_ALREADY_INIT | 已初始化 | 先调用 shell_deinit() |
| 5 | SHELL_ERROR_NO_MEMORY | 内存不足 | 减小配置或增加堆 |
| 6 | SHELL_ERROR_NOT_FOUND | 未找到 | 检查命令是否注册 |
| 7 | SHELL_ERROR_ALREADY_EXISTS | 已存在 | 使用不同的命令名 |
| 8 | SHELL_ERROR_NO_BACKEND | 无后端 | 调用 shell_set_backend() |
| 9 | SHELL_ERROR_BUFFER_FULL | 缓冲区满 | 增大缓冲区或减少命令 |

## 获取帮助

### 自助资源

1. **查看文档**：
   - [USER_GUIDE.md](USER_GUIDE.md) - 使用指南
   - [DESIGN.md](DESIGN.md) - 架构设计
   - [PORTING_GUIDE.md](PORTING_GUIDE.md) - 移植指南
   - [TEST_GUIDE.md](TEST_GUIDE.md) - 测试指南

2. **查看示例**：
   - `examples/stm32f4-shell/` - STM32 示例
   - `examples/esp32-shell/` - ESP32 示例
   - `examples/linux-shell/` - Linux 示例

3. **运行测试**：
   ```bash
   cd build
   ctest --verbose
   ```

### 社区支持

1. **GitHub Issues**：
   - 搜索已知问题
   - 提交新问题
   - 参与讨论

2. **论坛**：
   - Nexus 社区论坛
   - Stack Overflow (标签: nexus-shell)

3. **邮件列表**：
   - nexus-users@lists.nexus.org
   - nexus-dev@lists.nexus.org

### 提交问题

提交问题时请包含：

1. **环境信息**：
   - 平台和 CPU
   - 编译器版本
   - Shell 版本

2. **问题描述**：
   - 症状
   - 预期行为
   - 实际行为

3. **复现步骤**：
   - 详细的操作步骤
   - 最小复现代码

4. **相关日志**：
   - 错误信息
   - 调试输出
   - 崩溃堆栈

**示例**：

```
**环境**：
- 平台: STM32F407VG
- 编译器: arm-none-eabi-gcc 10.3.1
- Shell 版本: 1.0.0

**问题描述**：
Shell 初始化失败，返回 SHELL_ERROR_NO_MEMORY

**复现步骤**：
1. 调用 shell_init(&config)
2. 返回 SHELL_ERROR_NO_MEMORY

**配置**：
```c
shell_config_t config = {
    .prompt = "nexus> ",
    .cmd_buffer_size = 256,
    .history_depth = 32,
    .max_commands = 64
};
```

**日志**：
```
Free heap before init: 2048 bytes
Required memory: 8192 bytes
Init failed: SHELL_ERROR_NO_MEMORY
```
```

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
