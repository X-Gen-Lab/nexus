# Nexus Shell Framework

嵌入式平台交互式命令行界面（CLI）框架，提供功能丰富、可扩展的 Shell 环境。

## 特性

- **命令注册**: 动态注册自定义命令，支持帮助文本和用法说明
- **行编辑**: 支持光标移动、插入、删除、Home/End 键
- **命令历史**: 上下箭头浏览历史命令，可配置历史深度
- **自动补全**: Tab 键自动补全命令名和参数
- **内置命令**: help、version、clear、history、echo
- **多后端支持**: UART、Console 等可插拔后端
- **参数解析**: 支持引号、转义字符的参数解析
- **线程安全**: 多任务环境下安全使用
- **资源可配置**: 支持静态分配，适用于资源受限环境

## 快速开始

### 基本使用

```c
#include "shell/shell.h"

// 自定义命令处理函数
static int cmd_led(int argc, char* argv[]) {
    if (argc < 2) {
        shell_printf("Usage: led <on|off>\r\n");
        return -1;
    }
    
    if (strcmp(argv[1], "on") == 0) {
        hal_gpio_write(LED_PIN, 1);
        shell_printf("LED turned on\r\n");
    } else if (strcmp(argv[1], "off") == 0) {
        hal_gpio_write(LED_PIN, 0);
        shell_printf("LED turned off\r\n");
    }
    return 0;
}

// 命令定义
static const shell_command_t led_cmd = {
    .name = "led",
    .handler = cmd_led,
    .help = "Control LED state",
    .usage = "led <on|off>"
};

void app_init(void) {
    // 初始化 Shell
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    // 设置后端 (UART)
    shell_set_backend(&shell_uart_backend);
    
    // 注册内置命令
    shell_register_builtin_commands();
    
    // 注册自定义命令
    shell_register_command(&led_cmd);
    
    // 打印提示符
    shell_print_prompt();
}

void app_loop(void) {
    // 处理 Shell 输入 (非阻塞)
    shell_process();
}
```

### 自定义配置

```c
shell_config_t config = {
    .prompt = "nexus> ",        // 自定义提示符
    .cmd_buffer_size = 128,     // 命令缓冲区大小
    .history_depth = 16,        // 历史记录深度
    .max_commands = 64          // 最大命令数
};

shell_init(&config);
```

## API 参考

### 初始化与控制

| 函数 | 描述 |
|------|------|
| `shell_init(config)` | 初始化 Shell |
| `shell_deinit()` | 反初始化 Shell |
| `shell_is_initialized()` | 检查是否已初始化 |
| `shell_process()` | 处理输入（非阻塞） |
| `shell_get_version()` | 获取版本字符串 |

### 命令管理

| 函数 | 描述 |
|------|------|
| `shell_register_command(cmd)` | 注册命令 |
| `shell_unregister_command(name)` | 注销命令 |
| `shell_find_command(name)` | 查找命令 |
| `shell_get_command_count()` | 获取命令数量 |
| `shell_register_builtin_commands()` | 注册内置命令 |

### 输出函数

| 函数 | 描述 |
|------|------|
| `shell_printf(fmt, ...)` | 格式化输出 |
| `shell_puts(str)` | 输出字符串 |
| `shell_putc(c)` | 输出字符 |
| `shell_print_prompt()` | 打印提示符 |
| `shell_clear_screen()` | 清屏 |

### 后端管理

| 函数 | 描述 |
|------|------|
| `shell_set_backend(backend)` | 设置后端 |
| `shell_get_backend()` | 获取当前后端 |

### 历史管理

| 函数 | 描述 |
|------|------|
| `shell_get_history_manager()` | 获取历史管理器 |
| `history_add(mgr, cmd)` | 添加历史记录 |
| `history_get(mgr, index)` | 获取历史记录 |
| `history_clear(mgr)` | 清空历史 |

## 内置命令

| 命令 | 描述 | 用法 |
|------|------|------|
| `help` | 显示可用命令 | `help [command]` |
| `version` | 显示 Shell 版本 | `version` |
| `clear` | 清除终端屏幕 | `clear` |
| `history` | 显示命令历史 | `history` |
| `echo` | 打印参数 | `echo [text...]` |

## 命令定义

### 命令结构

```c
typedef struct {
    const char* name;           // 命令名称
    shell_cmd_handler_t handler; // 处理函数
    const char* help;           // 帮助文本
    const char* usage;          // 用法说明
    shell_completion_t completion; // 自动补全函数 (可选)
} shell_command_t;
```

### 命令处理函数

```c
typedef int (*shell_cmd_handler_t)(int argc, char* argv[]);
```

- `argc`: 参数数量（包括命令名）
- `argv`: 参数数组
- 返回值: 0 表示成功，非 0 表示错误

### 带自动补全的命令

```c
// 自动补全函数
static int led_completion(const char* partial, char* completions[], int max) {
    const char* options[] = {"on", "off", "toggle", "status"};
    int count = 0;
    
    for (int i = 0; i < 4 && count < max; i++) {
        if (strncmp(options[i], partial, strlen(partial)) == 0) {
            completions[count++] = (char*)options[i];
        }
    }
    return count;
}

static const shell_command_t led_cmd = {
    .name = "led",
    .handler = cmd_led,
    .help = "Control LED state",
    .usage = "led <on|off|toggle|status>",
    .completion = led_completion
};
```

## 行编辑快捷键

| 按键 | 功能 |
|------|------|
| `←` / `Ctrl+B` | 光标左移 |
| `→` / `Ctrl+F` | 光标右移 |
| `Home` / `Ctrl+A` | 移到行首 |
| `End` / `Ctrl+E` | 移到行尾 |
| `Backspace` | 删除前一字符 |
| `Delete` / `Ctrl+D` | 删除当前字符 |
| `Ctrl+K` | 删除到行尾 |
| `Ctrl+U` | 删除到行首 |
| `↑` / `Ctrl+P` | 上一条历史 |
| `↓` / `Ctrl+N` | 下一条历史 |
| `Tab` | 自动补全 |
| `Ctrl+C` | 取消当前输入 |
| `Ctrl+L` | 清屏 |

## 后端接口

### 后端结构

```c
typedef struct {
    int (*read)(char* buf, size_t len);   // 读取输入
    int (*write)(const char* buf, size_t len); // 写入输出
} shell_backend_t;
```

### UART 后端

```c
#include "shell/shell_backend.h"

// 初始化 UART
hal_uart_config_t uart_cfg = {
    .baudrate = 115200,
    .wordlen = HAL_UART_WORDLEN_8,
    .stopbits = HAL_UART_STOPBITS_1,
    .parity = HAL_UART_PARITY_NONE
};
hal_uart_init(HAL_UART_0, &uart_cfg);

// 使用 UART 后端
shell_set_backend(&shell_uart_backend);
```

### 自定义后端

```c
static int my_read(char* buf, size_t len) {
    // 实现读取逻辑
    return bytes_read;
}

static int my_write(const char* buf, size_t len) {
    // 实现写入逻辑
    return bytes_written;
}

static const shell_backend_t my_backend = {
    .read = my_read,
    .write = my_write
};

shell_set_backend(&my_backend);
```

## 错误处理

```c
shell_status_t status = shell_register_command(&cmd);
if (status != SHELL_OK) {
    const char* msg = shell_get_error_message(status);
    printf("Error: %s\n", msg);
}
```

### 状态码

| 状态码 | 描述 |
|--------|------|
| `SHELL_OK` | 成功 |
| `SHELL_ERROR_INVALID_PARAM` | 无效参数 |
| `SHELL_ERROR_NOT_INIT` | 未初始化 |
| `SHELL_ERROR_ALREADY_INIT` | 已初始化 |
| `SHELL_ERROR_NO_BACKEND` | 无后端 |
| `SHELL_ERROR_CMD_NOT_FOUND` | 命令未找到 |
| `SHELL_ERROR_CMD_EXISTS` | 命令已存在 |
| `SHELL_ERROR_BUFFER_FULL` | 缓冲区满 |

## 编译时配置

| 宏定义 | 默认值 | 描述 |
|--------|--------|------|
| `SHELL_MAX_COMMANDS` | `32` | 最大命令数 |
| `SHELL_DEFAULT_CMD_BUFFER_SIZE` | `128` | 默认命令缓冲区 |
| `SHELL_DEFAULT_HISTORY_DEPTH` | `8` | 默认历史深度 |
| `SHELL_MAX_ARGS` | `16` | 最大参数数量 |
| `SHELL_DEFAULT_PROMPT` | `"$ "` | 默认提示符 |

## 目录结构

```
framework/shell/
├── include/shell/
│   ├── shell.h              # 核心 API
│   ├── shell_def.h          # 类型定义和常量
│   ├── shell_command.h      # 命令管理
│   ├── shell_backend.h      # 后端接口
│   ├── shell_history.h      # 历史管理
│   ├── shell_line_editor.h  # 行编辑器
│   ├── shell_autocomplete.h # 自动补全
│   └── shell_parser.h       # 参数解析
├── src/
│   ├── shell.c              # 核心实现
│   ├── shell_command.c      # 命令管理
│   ├── shell_builtin.c      # 内置命令
│   ├── shell_backend.c      # 后端管理
│   ├── shell_history.c      # 历史实现
│   ├── shell_line_editor.c  # 行编辑器
│   ├── shell_autocomplete.c # 自动补全
│   └── shell_parser.c       # 参数解析
├── CMakeLists.txt
└── README.md
```

## 依赖

- **OSAL**: 操作系统抽象层（可选，用于线程安全）
- **HAL**: 硬件抽象层（UART 后端）

## 示例应用

完整示例请参考 `applications/shell_demo/`。

## 许可证

Copyright (c) 2026 Nexus Team
