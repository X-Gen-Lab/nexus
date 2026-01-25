# Nexus Shell Framework

嵌入式平台交互式命令行界面（CLI）框架，提供功能丰富、可扩展的 Shell 环境。

## 特性

- **命令注册**: 动态注册自定义命令，支持帮助文本和用法说明
- **行编辑**: 支持光标移动、插入、删除、Home/End 键
- **命令历史**: 上下箭头浏览历史命令，可配置历史深度（4-32 条）
- **自动补全**: Tab 键自动补全命令名和参数，支持自定义补全函数
- **内置命令**: help、version、clear、history、echo
- **多后端支持**: UART、Console、Mock 等可插拔后端
- **参数解析**: 支持引号、转义字符的参数解析
- **转义序列**: 完整支持 ANSI 转义序列（箭头键、Home/End 等）
- **非阻塞处理**: shell_process() 非阻塞，适合主循环和 RTOS
- **资源可配置**: 可配置缓冲区大小、历史深度、命令数量
- **跨平台**: 支持 ARM Cortex-M/A、RISC-V、x86/x64
- **完整测试**: 单元测试、集成测试、功能测试

## 概述

Shell Framework 是 Nexus 平台的交互式命令行界面，专为嵌入式环境设计，提供现代化的 Shell 体验。

### 核心优势

- ✅ **易用性** - 简单的 API，快速集成
- ✅ **功能丰富** - 行编辑、历史、补全等现代 Shell 特性
- ✅ **高性能** - 命令执行延迟 < 1ms，非阻塞处理
- ✅ **可扩展** - 灵活的命令注册和后端接口
- ✅ **可移植** - 标准 C99，支持多种平台和编译器
- ✅ **资源友好** - 内存占用 ~4KB，Flash 占用 ~12KB

### 性能指标

基于实际测试结果：

| 指标 | 值 | 状态 |
|------|---|------|
| **命令执行延迟** | < 1 ms | ✅ 达标 |
| **shell_process() 开销** | < 100 μs | ✅ 达标 |
| **内存占用** | ~4 KB | ✅ 达标 |
| **Flash 占用** | ~12 KB | ✅ 达标 |
| **最大命令数** | 32（可配置） | ✅ 达标 |
| **历史深度** | 4-32（可配置） | ✅ 达标 |

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

## 高级功能

### 参数解析

Shell 支持引号和转义字符的参数解析：

```c
// 支持引号包裹的参数
$ echo "Hello World"
Hello World

// 支持转义字符
$ echo "Line 1\nLine 2"
Line 1
Line 2

// 支持单引号
$ echo 'Single quotes'
Single quotes

// 混合使用
$ cmd arg1 "arg with spaces" arg3
```

### 自定义自动补全

为命令实现智能补全：

```c
static int gpio_completion(const char* partial, char* completions[], int max) {
    const char* pins[] = {"PA0", "PA1", "PB0", "PB1", "PC0"};
    int count = 0;
    
    for (int i = 0; i < 5 && count < max; i++) {
        if (strncmp(pins[i], partial, strlen(partial)) == 0) {
            completions[count++] = (char*)pins[i];
        }
    }
    return count;
}

static const shell_command_t gpio_cmd = {
    .name = "gpio",
    .handler = cmd_gpio,
    .help = "GPIO control",
    .usage = "gpio <pin> <high|low>",
    .completion = gpio_completion
};
```

### 命令分组

使用命名约定组织命令：

```c
// HAL 命令组
shell_register_command(&(shell_command_t){
    .name = "hal.gpio.read",
    .handler = cmd_gpio_read,
    .help = "Read GPIO pin"
});

shell_register_command(&(shell_command_t){
    .name = "hal.gpio.write",
    .handler = cmd_gpio_write,
    .help = "Write GPIO pin"
});

// 网络命令组
shell_register_command(&(shell_command_t){
    .name = "net.ping",
    .handler = cmd_ping,
    .help = "Ping remote host"
});
```

### 多行输入

支持反斜杠续行：

```c
$ echo "This is a very long \
> command that spans \
> multiple lines"
This is a very long command that spans multiple lines
```

### 命令别名

实现命令别名功能：

```c
static int cmd_alias(int argc, char* argv[]) {
    if (argc < 3) {
        shell_printf("Usage: alias <name> <command>\r\n");
        return -1;
    }
    
    // 保存别名映射
    alias_add(argv[1], argv[2]);
    return 0;
}
```

## 编译时配置

### 可配置参数

| 宏定义 | 默认值 | 描述 |
|--------|--------|------|
| `SHELL_MAX_COMMANDS` | `32` | 最大命令数 |
| `SHELL_DEFAULT_CMD_BUFFER_SIZE` | `128` | 默认命令缓冲区大小 |
| `SHELL_DEFAULT_HISTORY_DEPTH` | `8` | 默认历史深度 |
| `SHELL_MAX_ARGS` | `16` | 最大参数数量 |
| `SHELL_DEFAULT_PROMPT` | `"$ "` | 默认提示符 |
| `SHELL_ENABLE_COLORS` | `1` | 启用 ANSI 颜色 |
| `SHELL_ENABLE_AUTOCOMPLETE` | `1` | 启用自动补全 |
| `SHELL_ENABLE_HISTORY` | `1` | 启用历史功能 |
| `SHELL_ENABLE_LINE_EDITOR` | `1` | 启用行编辑器 |

### 裁剪配置

根据需求裁剪功能：

```cmake
# CMakeLists.txt - 最小配置
add_definitions(
    -DSHELL_MAX_COMMANDS=16
    -DSHELL_ENABLE_AUTOCOMPLETE=0
    -DSHELL_ENABLE_HISTORY=0
    -DSHELL_DEFAULT_CMD_BUFFER_SIZE=64
)
```

```cmake
# CMakeLists.txt - 完整配置
add_definitions(
    -DSHELL_MAX_COMMANDS=64
    -DSHELL_ENABLE_AUTOCOMPLETE=1
    -DSHELL_ENABLE_HISTORY=1
    -DSHELL_DEFAULT_HISTORY_DEPTH=32
    -DSHELL_DEFAULT_CMD_BUFFER_SIZE=256
)
```

## 最佳实践

### 命令设计

```c
// ✅ 好的命令设计
static int cmd_led(int argc, char* argv[]) {
    // 1. 参数验证
    if (argc < 2) {
        shell_printf("Usage: %s <on|off|toggle|status>\r\n", argv[0]);
        return -1;
    }
    
    // 2. 参数解析
    if (strcmp(argv[1], "on") == 0) {
        hal_gpio_write(LED_PIN, 1);
        shell_printf("LED turned on\r\n");
    } else if (strcmp(argv[1], "off") == 0) {
        hal_gpio_write(LED_PIN, 0);
        shell_printf("LED turned off\r\n");
    } else if (strcmp(argv[1], "status") == 0) {
        int state = hal_gpio_read(LED_PIN);
        shell_printf("LED is %s\r\n", state ? "on" : "off");
    } else {
        shell_printf("Error: Invalid argument '%s'\r\n", argv[1]);
        return -1;
    }
    
    return 0;
}

// ❌ 不好的命令设计
static int cmd_bad(int argc, char* argv[]) {
    // 没有参数验证
    hal_gpio_write(LED_PIN, atoi(argv[1]));  // 可能崩溃
    // 没有用户反馈
    return 0;
}
```

### 错误处理

```c
static int cmd_file_read(int argc, char* argv[]) {
    if (argc < 2) {
        shell_printf("Usage: read <filename>\r\n");
        return -1;
    }
    
    FILE* fp = fopen(argv[1], "r");
    if (!fp) {
        shell_printf("Error: Cannot open file '%s'\r\n", argv[1]);
        return -1;
    }
    
    char buf[128];
    while (fgets(buf, sizeof(buf), fp)) {
        shell_printf("%s", buf);
    }
    
    fclose(fp);
    return 0;
}
```

### 长时间操作

对于长时间操作，提供进度反馈：

```c
static int cmd_flash_erase(int argc, char* argv[]) {
    shell_printf("Erasing flash...\r\n");
    
    for (int i = 0; i < 100; i++) {
        hal_flash_erase_sector(i);
        
        // 每 10% 显示进度
        if (i % 10 == 0) {
            shell_printf("Progress: %d%%\r\n", i);
        }
    }
    
    shell_printf("Flash erase complete\r\n");
    return 0;
}
```

### RTOS 集成

在 FreeRTOS 中使用：

```c
void shell_task(void* param) {
    // 初始化 Shell
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    shell_set_backend(&shell_uart_backend);
    shell_register_builtin_commands();
    
    // 注册应用命令
    register_app_commands();
    
    shell_print_prompt();
    
    // 主循环
    while (1) {
        shell_process();
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 轮询
    }
}

void app_main(void) {
    xTaskCreate(shell_task, "shell", 2048, NULL, 5, NULL);
}
```

## 依赖

### 必需依赖

- **标准库**: `stdio.h`, `string.h`, `stdarg.h`, `ctype.h`
- **HAL**: 硬件抽象层
  - UART: UART 后端需要
  - GPIO: GPIO 相关命令需要（可选）

### 可选依赖

- **OSAL**: 操作系统抽象层
  - Mutex: 多线程环境下的线程安全（可选）
  - Task: RTOS 任务创建（可选）

- **ANSI 支持**: 终端支持 ANSI 转义序列（行编辑、颜色）

## 完整文档

完整文档请参考 `docs/` 目录：

- **[docs/README.md](docs/README.md)** - 文档索引和导航
  - 文档结构说明
  - 使用建议（新手入门、架构理解、移植开发等）
  - 模块概述和性能指标
  - 支持的平台和编译器

- **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)** - 详细使用指南（42+ 页）
  - 快速开始和基本操作
  - 命令管理（注册、注销、查找、内置命令）
  - 行编辑功能（光标移动、删除、剪切、粘贴）
  - 历史管理（浏览、搜索、持久化）
  - 自动补全（命令补全、参数补全、自定义补全）
  - 后端配置（UART、Console、Mock、自定义）
  - 参数解析（引号、转义、空格处理）
  - 输出函数（printf、puts、putc、格式化）
  - 高级功能（命令别名、多行输入、管道、重定向）
  - 最佳实践和常见问题

- **[docs/DESIGN.md](docs/DESIGN.md)** - 架构设计文档（42+ 页）
  - 设计目标和核心特性
  - 系统架构（分层设计、模块划分）
  - 核心数据结构（Shell 实例、命令表、历史缓冲区）
  - 关键流程（初始化、输入处理、命令执行、输出）
  - 命令注册机制（静态注册、动态注册、命令查找）
  - 行编辑器设计（状态机、光标管理、缓冲区操作）
  - 历史管理（环形缓冲区、历史导航、去重）
  - 自动补全（前缀匹配、模糊匹配、多候选处理）
  - 后端接口（读写接口、阻塞/非阻塞模式）
  - 转义序列处理（ANSI 解析、状态机实现）
  - 线程安全（互斥保护、可重入性）
  - 内存管理（静态/动态分配、内存池）
  - 性能优化（缓存、批量处理、零拷贝）
  - 设计权衡和未来改进方向

- **[docs/TEST_GUIDE.md](docs/TEST_GUIDE.md)** - 测试指南（37+ 页）
  - 测试策略（单元/集成/功能/性能）
  - 测试环境搭建（工具、依赖、配置）
  - 单元测试（初始化、命令管理、行编辑、历史、补全）
  - 集成测试（完整工作流、多后端、RTOS 集成）
  - 功能测试（用户场景、边界条件、错误处理）
  - 性能测试（命令执行延迟、吞吐量、内存占用）
  - Mock 后端测试（输入模拟、输出验证）
  - 测试工具和辅助函数
  - 持续集成配置
  - 测试最佳实践

- **[docs/PORTING_GUIDE.md](docs/PORTING_GUIDE.md)** - 移植指南（33+ 页）
  - 移植概述（依赖项、可移植性设计、工作量评估）
  - 依赖项分析（标准库、HAL、OSAL）
  - 平台适配
    - ARM Cortex-M（STM32、NXP、Nordic）
    - ARM Cortex-A（Raspberry Pi、i.MX）
    - RISC-V（SiFive、Kendryte）
    - x86/x64（Native、Linux、Windows）
  - 后端实现（UART、Console、USB CDC、网络）
  - 编译配置（CMake、Kconfig、Makefile）
  - 详细移植步骤（准备、实现、验证）
  - 平台特定优化（内存、性能、功耗）
  - 验证清单和测试
  - 故障排查
  - 示例项目

- **[docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** - 故障排查指南（30+ 页）
  - 初始化问题（初始化失败、后端设置失败）
  - 输入输出问题（无输入、无输出、乱码、延迟）
  - 命令执行问题（命令未找到、参数错误、执行失败）
  - 行编辑问题（光标错位、删除异常、显示错误）
  - 历史功能问题（历史丢失、导航异常、重复记录）
  - 自动补全问题（补全失败、候选错误、性能问题）
  - 后端问题（UART 配置、波特率、流控）
  - 性能问题（响应慢、CPU 占用高、内存泄漏）
  - 内存问题（栈溢出、堆溢出、碎片化）
  - 调试技巧（使能调试输出、使用 Mock 后端、性能分析）
  - 常见错误码速查
  - 获取帮助

- **[docs/CHANGELOG.md](docs/CHANGELOG.md)** - 版本变更记录（7+ 页）
  - 版本 1.0.0（2026-01-24）
    - 新增功能（命令注册、行编辑、历史、补全）
    - 性能指标（命令延迟 < 1ms、内存 ~4KB）
    - 已知限制（最大命令数、历史深度）
    - 依赖项（HAL、OSAL）
    - 兼容性（编译器、架构、RTOS）
    - 升级指南
  - 未来版本计划（脚本支持、远程 Shell、安全增强）

## 示例应用

完整示例请参考：

- **`applications/shell_demo/`** - Shell 演示应用
  - 基本命令示例
  - 自定义命令实现
  - 后端配置示例
  - RTOS 集成示例

## 测试覆盖

Shell Framework 拥有完整的测试套件：

| 测试类型 | 测试数量 | 覆盖内容 |
|---------|---------|---------|
| **单元测试** | 45 | 初始化、命令管理、行编辑、历史、补全、解析 |
| **集成测试** | 12 | 完整工作流、多后端、RTOS 集成 |
| **功能测试** | 18 | 用户场景、边界条件、错误处理 |
| **性能测试** | 8 | 命令延迟、吞吐量、内存占用 |
| **总计** | **83** | **100% 通过率** |

**覆盖率指标**:
- ✅ 行覆盖率: ≥ 90%
- ✅ 分支覆盖率: ≥ 85%
- ✅ 函数覆盖率: 100%

详细测试文档请参考：
- [tests/shell/README.md](../../tests/shell/README.md) - 测试套件说明
- [docs/TEST_GUIDE.md](docs/TEST_GUIDE.md) - 测试指南

## 支持的平台

### 编译器

| 编译器 | 版本 | 状态 |
|--------|------|------|
| GCC | ≥ 7.0 | ✅ 完全支持 |
| Clang | ≥ 8.0 | ✅ 完全支持 |
| MSVC | ≥ 2019 | ✅ 完全支持 |
| Arm Compiler 5 | ≥ 5.06 | ✅ 完全支持 |
| Arm Compiler 6 | ≥ 6.0 | ✅ 完全支持 |
| IAR | ≥ 8.0 | ✅ 完全支持 |

### 架构

| 架构 | 状态 | 备注 |
|------|------|------|
| ARM Cortex-M | ✅ 完全支持 | STM32、NXP、Nordic 等 |
| ARM Cortex-A | ✅ 完全支持 | Raspberry Pi、i.MX 等 |
| RISC-V | ✅ 完全支持 | SiFive、Kendryte 等 |
| x86/x64 | ✅ 完全支持 | Native、Linux、Windows |

### RTOS

| RTOS | 状态 | 备注 |
|------|------|------|
| FreeRTOS | ✅ 完全支持 | 推荐任务优先级 5 |
| RT-Thread | ✅ 完全支持 | 推荐线程优先级 10 |
| Zephyr | ✅ 完全支持 | 推荐线程优先级 5 |
| 裸机 | ✅ 完全支持 | 主循环轮询模式 |

### 终端

| 终端 | 状态 | 备注 |
|------|------|------|
| PuTTY | ✅ 完全支持 | 推荐配置：VT100 模式 |
| SecureCRT | ✅ 完全支持 | 推荐配置：ANSI 颜色 |
| Tera Term | ✅ 完全支持 | 推荐配置：UTF-8 编码 |
| minicom | ✅ 完全支持 | Linux 串口工具 |
| screen | ✅ 完全支持 | Linux/macOS 串口工具 |
| VS Code Terminal | ✅ 完全支持 | 内置终端 |

## 性能基准

基于实际测试结果（ARM Cortex-M4 @ 168MHz）：

| 指标 | 值 | 备注 |
|------|---|------|
| **命令执行延迟** | < 1 ms | 从输入到执行 |
| **shell_process() 开销** | < 100 μs | 单次调用 |
| **内存占用（RAM）** | ~4 KB | 包含缓冲区和历史 |
| **内存占用（Flash）** | ~12 KB | 完整功能 |
| **最大命令数** | 32（可配置） | 默认配置 |
| **历史深度** | 4-32（可配置） | 默认 8 条 |
| **命令缓冲区** | 128 字节（可配置） | 默认配置 |
| **吞吐量** | > 1000 cmd/s | 简单命令 |

## 获取帮助

### 问题反馈

如果遇到问题，请按以下顺序查找解决方案：

1. **查看 [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)** - 常见问题和解决方案
2. **查看 [USER_GUIDE.md](docs/USER_GUIDE.md)** - 使用指南相关章节
3. **查看 [DESIGN.md](docs/DESIGN.md)** - 了解内部机制
4. **搜索已知问题** - 检查 GitHub Issues
5. **提交新问题** - 提供详细的复现步骤

### 提交问题时请包含

- Shell 版本号（`version` 命令输出）
- 平台信息（MCU 型号、RTOS、编译器）
- 配置信息（`shell_config_t` 设置）
- 复现步骤（详细的操作步骤）
- 预期行为和实际行为
- 相关日志输出

### 联系方式

- 📧 Email: support@nexus-team.com
- 🐛 Issues: https://github.com/nexus/shell/issues
- 💬 Discussions: https://github.com/nexus/shell/discussions
- 📖 Wiki: https://github.com/nexus/shell/wiki

## 贡献指南

欢迎贡献代码和文档改进：

1. Fork 项目仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

请确保：
- 遵循 Nexus 代码注释规范
- 添加相应的测试用例
- 更新相关文档
- 所有测试通过
- 代码覆盖率 ≥ 90%

详细贡献指南请参考 [CONTRIBUTING.md](../../CONTRIBUTING.md)。

## 常见问题 (FAQ)

### Q: Shell 支持哪些终端？

A: Shell 支持所有支持 ANSI 转义序列的终端，包括 PuTTY、SecureCRT、Tera Term、minicom、screen 等。

### Q: 如何禁用颜色输出？

A: 在配置中设置 `config.color_enabled = false`，或编译时定义 `SHELL_ENABLE_COLORS=0`。

### Q: 如何增加命令缓冲区大小？

A: 在配置中设置 `config.cmd_buffer_size = 256`，或编译时定义 `SHELL_DEFAULT_CMD_BUFFER_SIZE=256`。

### Q: Shell 是否线程安全？

A: 基本功能是线程安全的。如果需要在多线程环境中使用，建议启用 OSAL Mutex 保护。

### Q: 如何实现命令权限控制？

A: 在命令处理函数中检查权限，例如：

```c
static int cmd_admin(int argc, char* argv[]) {
    if (!is_admin_logged_in()) {
        shell_printf("Error: Admin privilege required\r\n");
        return -1;
    }
    // 执行管理员命令
    return 0;
}
```

### Q: 如何保存命令历史到 Flash？

A: 实现历史持久化功能：

```c
void save_history_to_flash(void) {
    shell_history_manager_t* mgr = shell_get_history_manager();
    for (int i = 0; i < mgr->count; i++) {
        const char* cmd = history_get(mgr, i);
        flash_write(HISTORY_ADDR + i * 128, cmd, strlen(cmd));
    }
}

void load_history_from_flash(void) {
    shell_history_manager_t* mgr = shell_get_history_manager();
    char buf[128];
    for (int i = 0; i < HISTORY_MAX; i++) {
        flash_read(HISTORY_ADDR + i * 128, buf, 128);
        if (buf[0] != 0xFF) {
            history_add(mgr, buf);
        }
    }
}
```

### Q: 如何实现远程 Shell（网络）？

A: 实现网络后端：

```c
static int net_read(char* buf, size_t len) {
    return socket_recv(shell_socket, buf, len, 0);
}

static int net_write(const char* buf, size_t len) {
    return socket_send(shell_socket, buf, len, 0);
}

static const shell_backend_t net_backend = {
    .read = net_read,
    .write = net_write
};

shell_set_backend(&net_backend);
```

### Q: 如何减少内存占用？

A: 裁剪不需要的功能：

```cmake
add_definitions(
    -DSHELL_ENABLE_AUTOCOMPLETE=0    # 禁用自动补全
    -DSHELL_ENABLE_HISTORY=0         # 禁用历史
    -DSHELL_MAX_COMMANDS=16          # 减少最大命令数
    -DSHELL_DEFAULT_CMD_BUFFER_SIZE=64  # 减少缓冲区
)
```

## 许可证

Copyright (c) 2026 Nexus Team

---

**版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
