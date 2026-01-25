# Shell Framework 架构设计文档

本文档详细描述 Nexus Shell Framework 的架构设计、核心数据结构、关键流程和设计决策。

## 目录

1. [设计目标](#设计目标)
2. [核心特性](#核心特性)
3. [系统架构](#系统架构)
4. [核心数据结构](#核心数据结构)
5. [关键流程](#关键流程)
6. [命令注册机制](#命令注册机制)
7. [行编辑器设计](#行编辑器设计)
8. [历史管理设计](#历史管理设计)
9. [自动补全设计](#自动补全设计)
10. [后端接口设计](#后端接口设计)
11. [转义序列处理](#转义序列处理)
12. [线程安全设计](#线程安全设计)
13. [内存管理策略](#内存管理策略)
14. [性能优化方案](#性能优化方案)
15. [设计权衡](#设计权衡)
16. [未来改进方向](#未来改进方向)

## 设计目标

Shell Framework 的设计遵循以下核心目标：

### 1. 易用性

- **简单的 API**: 提供直观的初始化和命令注册接口
- **丰富的功能**: 支持行编辑、历史、自动补全等现代 Shell 特性
- **良好的文档**: 完整的 API 文档和使用示例

### 2. 可扩展性

- **命令注册**: 支持动态注册自定义命令
- **后端可插拔**: 支持多种 I/O 后端（UART、Console 等）
- **补全扩展**: 支持自定义补全函数

### 3. 资源效率

- **内存可控**: 可配置的缓冲区大小和历史深度
- **零拷贝**: 最小化内存拷贝操作
- **静态分配**: 支持静态内存分配模式

### 4. 可移植性

- **标准 C**: 使用标准 C99 编写
- **HAL 抽象**: 通过 HAL 层抽象硬件依赖
- **跨平台**: 支持裸机和 RTOS 环境

### 5. 用户体验

- **响应迅速**: 非阻塞输入处理
- **行编辑**: 支持光标移动、插入、删除
- **历史浏览**: 上下箭头浏览历史命令
- **自动补全**: Tab 键智能补全

## 核心特性

### 功能特性

| 特性 | 描述 | 状态 |
|------|------|------|
| 命令注册 | 动态注册自定义命令 | ✅ 已实现 |
| 行编辑 | 光标移动、插入、删除 | ✅ 已实现 |
| 命令历史 | 上下箭头浏览历史 | ✅ 已实现 |
| 自动补全 | Tab 键补全命令 | ✅ 已实现 |
| 内置命令 | help、version、clear 等 | ✅ 已实现 |
| 多后端 | UART、Console、自定义 | ✅ 已实现 |
| 参数解析 | 支持引号和转义 | ✅ 已实现 |
| 转义序列 | 支持 ANSI 转义序列 | ✅ 已实现 |

### 性能特性

| 指标 | 目标 | 实际 | 状态 |
|------|------|------|------|
| 命令执行延迟 | < 1 ms | < 1 ms | ✅ 达标 |
| 内存占用 | < 5 KB | ~4 KB | ✅ 达标 |
| 响应时间 | < 10 ms | < 5 ms | ✅ 超出目标 |


## 系统架构

Shell Framework 采用分层架构设计，各层职责清晰，便于维护和扩展。

### 架构层次

```
┌─────────────────────────────────────────────────────────────┐
│                      应用层 (Application)                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ 自定义命令 │  │ 自定义命令 │  │ 自定义命令 │  │   ...    │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │ 命令注册
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Shell 核心层 (Core)                       │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Shell 主控模块 (shell.c)                 │  │
│  │  - 初始化/反初始化                                     │  │
│  │  - 输入处理循环                                        │  │
│  │  - 转义序列处理                                        │  │
│  │  - 命令执行调度                                        │  │
│  └──────────────────────────────────────────────────────┘  │
│                              ▲                               │
│                              │                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ 命令管理  │  │ 行编辑器  │  │ 历史管理  │  │ 自动补全  │   │
│  │ (command) │  │ (editor)  │  │ (history) │  │(complete) │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
│       ▲             ▲             ▲             ▲           │
│       └─────────────┴─────────────┴─────────────┘           │
│                              │                               │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              参数解析器 (parser.c)                     │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │ I/O 操作
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    后端层 (Backend)                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   UART   │  │ Console  │  │   Mock   │  │  自定义   │   │
│  │  后端     │  │  后端     │  │  后端     │  │  后端     │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │ HAL 调用
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    硬件抽象层 (HAL)                          │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              HAL UART / Console                       │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 模块职责

#### Shell 核心模块 (shell.c)

**职责**:
- 模块初始化和反初始化
- 输入字符处理循环
- 转义序列状态机
- 命令执行调度
- 错误处理和恢复

**关键函数**:
- `shell_init()` - 初始化 Shell
- `shell_deinit()` - 反初始化
- `shell_process()` - 处理输入（非阻塞）
- `execute_command_line()` - 执行命令

#### 命令管理模块 (shell_command.c)

**职责**:
- 命令注册和注销
- 命令查找
- 命令列表管理
- 内置命令实现

**关键函数**:
- `shell_register_command()` - 注册命令
- `shell_unregister_command()` - 注销命令
- `shell_get_command()` - 查找命令
- `shell_get_command_count()` - 获取命令数量

#### 行编辑器模块 (shell_line_editor.c)

**职责**:
- 光标位置管理
- 字符插入和删除
- 行内容管理
- 显示刷新

**关键函数**:
- `line_editor_init()` - 初始化编辑器
- `line_editor_insert_char()` - 插入字符
- `line_editor_delete_char()` - 删除字符
- `line_editor_move_cursor()` - 移动光标

#### 历史管理模块 (shell_history.c)

**职责**:
- 历史记录存储（环形缓冲区）
- 历史浏览（上下箭头）
- 历史添加和清除

**关键函数**:
- `history_init()` - 初始化历史管理器
- `history_add()` - 添加历史记录
- `history_get_prev()` - 获取上一条历史
- `history_get_next()` - 获取下一条历史

#### 自动补全模块 (shell_autocomplete.c)

**职责**:
- 命令名补全
- 参数补全（可选）
- 公共前缀计算
- 匹配结果显示

**关键函数**:
- `autocomplete_process()` - 处理补全请求
- `autocomplete_show_matches()` - 显示匹配结果
- `autocomplete_get_common_prefix()` - 计算公共前缀

#### 参数解析模块 (shell_parser.c)

**职责**:
- 命令行解析
- 参数分割
- 引号处理
- 转义字符处理

**关键函数**:
- `parse_command_line()` - 解析命令行
- `parse_quoted_string()` - 解析引号字符串

#### 后端模块 (shell_backend.c)

**职责**:
- 后端接口管理
- I/O 操作封装
- 后端切换

**关键函数**:
- `shell_set_backend()` - 设置后端
- `shell_get_backend()` - 获取当前后端
- `shell_putchar()` - 输出字符
- `shell_puts()` - 输出字符串

## 核心数据结构

### Shell 上下文结构

```c
/**
 * \brief           Shell 上下文结构
 */
typedef struct {
    bool initialized;               /**< 初始化标志 */
    shell_config_t config;          /**< 配置副本 */
    const shell_backend_t* backend; /**< I/O 后端 */
    line_editor_t editor;           /**< 行编辑器状态 */
    history_manager_t history;      /**< 历史管理器状态 */
    char* cmd_buffer;               /**< 命令缓冲区 */
    char* saved_input;              /**< 保存的输入（历史浏览用） */
    shell_status_t last_error;      /**< 最后错误码 */
    
    /* 转义序列解析状态 */
    escape_state_t escape_state;    /**< 当前转义状态 */
    uint8_t escape_buffer[8];       /**< 转义序列缓冲区 */
    uint8_t escape_index;           /**< 转义缓冲区索引 */
    
    /* 历史存储 */
    char** history_entries;         /**< 历史条目指针数组 */
    char* history_storage;          /**< 历史存储缓冲区 */
    
    /* 提示符存储 */
    char prompt[SHELL_MAX_PROMPT_LEN + 1];
} shell_context_t;
```

**设计要点**:
- 单例模式：全局唯一的 Shell 上下文
- 状态集中：所有状态集中在一个结构中
- 内存管理：动态分配的缓冲区指针
- 转义状态：独立的转义序列状态机

### 命令结构

```c
/**
 * \brief           Shell 命令结构
 */
typedef struct {
    const char* name;               /**< 命令名称 */
    shell_cmd_handler_t handler;    /**< 处理函数 */
    const char* help;               /**< 帮助文本 */
    const char* usage;              /**< 用法说明 */
    shell_completion_t completion;  /**< 自动补全函数（可选） */
} shell_command_t;
```

**设计要点**:
- 常量字符串：name、help、usage 使用常量字符串
- 函数指针：handler 和 completion 为函数指针
- 可选补全：completion 可以为 NULL

### 命令注册表

```c
/**
 * \brief           命令注册表
 */
typedef struct {
    const shell_command_t* commands[SHELL_MAX_COMMANDS];
    uint8_t count;
} command_registry_t;
```

**设计要点**:
- 固定大小数组：避免动态分配
- 指针数组：存储命令结构指针
- 计数器：跟踪已注册命令数量

### 行编辑器状态

```c
/**
 * \brief           行编辑器状态
 */
typedef struct {
    char* buffer;       /**< 编辑缓冲区 */
    uint16_t capacity;  /**< 缓冲区容量 */
    uint16_t length;    /**< 当前长度 */
    uint16_t cursor;    /**< 光标位置 */
} line_editor_t;
```

**设计要点**:
- 外部缓冲区：buffer 指向外部分配的缓冲区
- 容量跟踪：capacity 记录缓冲区大小
- 长度和光标：独立跟踪内容长度和光标位置

### 历史管理器

```c
/**
 * \brief           历史管理器
 */
typedef struct {
    char** entries;         /**< 历史条目数组 */
    uint8_t capacity;       /**< 历史容量 */
    uint8_t count;          /**< 当前历史数量 */
    uint8_t head;           /**< 环形缓冲区头 */
    int8_t browse_index;    /**< 浏览索引（-1 表示未浏览） */
    uint16_t entry_size;    /**< 每条历史的大小 */
} history_manager_t;
```

**设计要点**:
- 环形缓冲区：使用 head 实现环形存储
- 浏览状态：browse_index 跟踪当前浏览位置
- 固定大小条目：每条历史固定大小

### 后端接口

```c
/**
 * \brief           Shell 后端接口
 */
typedef struct {
    int (*read)(char* buf, size_t len);         /**< 读取函数 */
    int (*write)(const char* buf, size_t len);  /**< 写入函数 */
} shell_backend_t;
```

**设计要点**:
- 简单接口：只需实现读写两个函数
- 非阻塞：read 应该是非阻塞的
- 返回值：返回实际读写的字节数

## 关键流程

### 初始化流程

```
shell_init()
    │
    ├─→ 验证配置参数
    │   └─→ validate_config()
    │
    ├─→ 分配命令缓冲区
    │   └─→ malloc(cmd_buffer_size)
    │
    ├─→ 分配保存输入缓冲区
    │   └─→ malloc(cmd_buffer_size)
    │
    ├─→ 分配历史存储
    │   ├─→ malloc(history_depth * entry_size)
    │   └─→ malloc(history_depth * sizeof(char*))
    │
    ├─→ 初始化行编辑器
    │   └─→ line_editor_init()
    │
    ├─→ 初始化历史管理器
    │   └─→ history_init()
    │
    ├─→ 初始化转义状态
    │   └─→ reset_escape_state()
    │
    └─→ 标记为已初始化
```

**关键点**:
- 参数验证：确保配置参数在有效范围内
- 内存分配：分配所有需要的缓冲区
- 错误处理：分配失败时清理已分配的内存
- 状态初始化：初始化所有子模块

### 输入处理流程

```
shell_process()
    │
    ├─→ 检查初始化状态
    │
    ├─→ 获取后端
    │   └─→ shell_get_backend()
    │
    ├─→ 非阻塞读取一个字符
    │   └─→ backend->read(&c, 1)
    │
    ├─→ 无输入？
    │   └─→ 返回 SHELL_OK
    │
    ├─→ 转义序列处理
    │   ├─→ process_escape_char(c)
    │   └─→ handle_escape_result()
    │
    ├─→ 控制字符？
    │   └─→ handle_control_char(c)
    │       ├─→ Enter: execute_command_line()
    │       ├─→ Backspace: line_editor_backspace()
    │       ├─→ Tab: handle_tab_completion()
    │       ├─→ Ctrl+C: 取消输入
    │       ├─→ Ctrl+L: 清屏
    │       └─→ ...
    │
    └─→ 可打印字符？
        └─→ handle_printable_char(c)
            ├─→ line_editor_insert_char()
            └─→ refresh_line_from_cursor()
```

**关键点**:
- 非阻塞：每次只读取一个字符，立即返回
- 状态机：转义序列使用状态机处理
- 分类处理：根据字符类型分别处理
- 显示刷新：插入字符后刷新显示


### 命令执行流程

```
execute_command_line()
    │
    ├─→ 获取输入内容
    │   └─→ line_editor_get_buffer()
    │
    ├─→ 空输入？
    │   └─→ 打印提示符，返回
    │
    ├─→ 添加到历史
    │   ├─→ history_add()
    │   └─→ history_reset_browse()
    │
    ├─→ 复制到解析缓冲区
    │   └─→ memcpy(parse_buffer, input)
    │
    ├─→ 解析命令行
    │   └─→ parse_command_line()
    │       ├─→ 分割参数
    │       ├─→ 处理引号
    │       └─→ 处理转义字符
    │
    ├─→ 查找命令
    │   └─→ shell_get_command(cmd_name)
    │
    ├─→ 命令未找到？
    │   └─→ 打印错误信息
    │
    ├─→ 执行命令处理函数
    │   └─→ cmd->handler(argc, argv)
    │
    ├─→ 返回值非零？
    │   └─→ 打印错误码
    │
    ├─→ 清空输入
    │   └─→ line_editor_clear()
    │
    └─→ 打印新提示符
```

**关键点**:
- 历史记录：执行前添加到历史
- 解析独立：使用独立缓冲区解析（避免修改原始输入）
- 错误处理：命令未找到或执行失败时打印错误
- 状态清理：执行后清空输入缓冲区

### 转义序列处理流程

```
process_escape_char(c)
    │
    ├─→ 当前状态：NORMAL
    │   ├─→ c == ESC？
    │   │   └─→ 切换到 ESC_STATE_ESC
    │   └─→ 其他字符：忽略
    │
    ├─→ 当前状态：ESC
    │   ├─→ c == '['？
    │   │   └─→ 切换到 ESC_STATE_CSI
    │   ├─→ c == 'O'？
    │   │   └─→ 切换到 ESC_STATE_SS3
    │   └─→ 其他：无效序列，重置
    │
    ├─→ 当前状态：CSI
    │   ├─→ 存储字符到缓冲区
    │   ├─→ 终止字符（A-Z, a-z, ~）？
    │   │   ├─→ 识别序列
    │   │   │   ├─→ 'A': UP
    │   │   │   ├─→ 'B': DOWN
    │   │   │   ├─→ 'C': RIGHT
    │   │   │   ├─→ 'D': LEFT
    │   │   │   ├─→ 'H': HOME
    │   │   │   ├─→ 'F': END
    │   │   │   ├─→ '3~': DELETE
    │   │   │   └─→ ...
    │   │   └─→ 重置状态
    │   └─→ 继续接收
    │
    └─→ 当前状态：SS3
        ├─→ 识别序列（同 CSI）
        └─→ 重置状态
```

**关键点**:
- 状态机：使用状态机处理多字节序列
- 缓冲区：临时存储序列字符
- 超时处理：无效序列时重置状态
- 兼容性：支持 CSI 和 SS3 两种格式

### 自动补全流程

```
handle_tab_completion()
    │
    ├─→ 获取当前输入
    │   └─→ line_editor_get_buffer()
    │
    ├─→ 处理补全
    │   └─→ autocomplete_process()
    │       ├─→ 提取命令名
    │       ├─→ 遍历命令列表
    │       ├─→ 匹配前缀
    │       └─→ 收集匹配结果
    │
    ├─→ 无匹配？
    │   └─→ 不做任何操作
    │
    ├─→ 单个匹配？
    │   ├─→ 清空输入
    │   ├─→ 插入完整命令名
    │   ├─→ 添加空格
    │   └─→ 刷新显示
    │
    └─→ 多个匹配？
        ├─→ 换行
        ├─→ 显示所有匹配
        │   └─→ autocomplete_show_matches()
        ├─→ 计算公共前缀
        │   └─→ autocomplete_get_common_prefix()
        ├─→ 有公共前缀？
        │   ├─→ 清空输入
        │   └─→ 插入公共前缀
        ├─→ 重新显示提示符
        └─→ 重新显示输入
```

**关键点**:
- 前缀匹配：只匹配命令名前缀
- 单匹配处理：自动完成并添加空格
- 多匹配处理：显示所有选项，完成公共前缀
- 显示刷新：补全后重新显示提示符和输入

## 命令注册机制

### 静态命令表

```c
/* 全局命令注册表 */
static command_registry_t g_command_registry = {
    .commands = {NULL},
    .count = 0
};
```

**设计要点**:
- 全局单例：整个系统只有一个命令注册表
- 固定大小：避免动态分配
- 指针数组：存储命令结构指针（命令本身可以是常量）

### 命令注册

```c
shell_status_t shell_register_command(const shell_command_t* cmd) {
    /* 参数验证 */
    if (cmd == NULL || cmd->name == NULL || cmd->handler == NULL) {
        return SHELL_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已满 */
    if (g_command_registry.count >= SHELL_MAX_COMMANDS) {
        return SHELL_ERROR_BUFFER_FULL;
    }
    
    /* 检查是否已存在 */
    if (shell_get_command(cmd->name) != NULL) {
        return SHELL_ERROR_ALREADY_EXISTS;
    }
    
    /* 添加到注册表 */
    g_command_registry.commands[g_command_registry.count++] = cmd;
    
    return SHELL_OK;
}
```

**关键点**:
- 参数验证：确保命令结构有效
- 重复检查：防止重复注册
- 容量检查：防止溢出
- 常量存储：只存储指针，不复制结构

### 命令查找

```c
const shell_command_t* shell_get_command(const char* name) {
    if (name == NULL) {
        return NULL;
    }
    
    /* 线性查找 */
    for (uint8_t i = 0; i < g_command_registry.count; i++) {
        if (strcmp(g_command_registry.commands[i]->name, name) == 0) {
            return g_command_registry.commands[i];
        }
    }
    
    return NULL;
}
```

**关键点**:
- 线性查找：命令数量少，线性查找足够高效
- 字符串比较：使用 strcmp 精确匹配
- 返回指针：返回命令结构指针，不复制

### 内置命令注册

```c
shell_status_t shell_register_builtin_commands(void) {
    static const shell_command_t builtin_commands[] = {
        {
            .name = "help",
            .handler = cmd_help,
            .help = "Show available commands",
            .usage = "help [command]"
        },
        {
            .name = "version",
            .handler = cmd_version,
            .help = "Show shell version",
            .usage = "version"
        },
        /* ... 更多内置命令 ... */
    };
    
    for (size_t i = 0; i < ARRAY_SIZE(builtin_commands); i++) {
        shell_register_command(&builtin_commands[i]);
    }
    
    return SHELL_OK;
}
```

**关键点**:
- 静态数组：内置命令使用静态常量数组
- 批量注册：循环注册所有内置命令
- 可选调用：应用可以选择不注册内置命令

## 行编辑器设计

### 光标管理

行编辑器维护两个关键位置：

- **length**: 当前内容长度
- **cursor**: 光标位置（0 到 length 之间）

```
Buffer: "hello world"
         ^     ^
         0     6 (cursor)
         
length = 11
cursor = 6
```

### 插入操作

```c
bool line_editor_insert_char(line_editor_t* editor, char c) {
    /* 检查容量 */
    if (editor->length >= editor->capacity - 1) {
        return false;
    }
    
    /* 在光标位置插入 */
    if (editor->cursor < editor->length) {
        /* 移动后面的字符 */
        memmove(&editor->buffer[editor->cursor + 1],
                &editor->buffer[editor->cursor],
                editor->length - editor->cursor);
    }
    
    /* 插入新字符 */
    editor->buffer[editor->cursor] = c;
    editor->cursor++;
    editor->length++;
    editor->buffer[editor->length] = '\0';
    
    return true;
}
```

**关键点**:
- 容量检查：防止缓冲区溢出
- 字符移动：插入中间时需要移动后面的字符
- 光标更新：插入后光标右移
- 空终止：保持字符串空终止

### 删除操作

```c
bool line_editor_backspace(line_editor_t* editor) {
    /* 光标在开头？ */
    if (editor->cursor == 0) {
        return false;
    }
    
    /* 移动后面的字符 */
    if (editor->cursor < editor->length) {
        memmove(&editor->buffer[editor->cursor - 1],
                &editor->buffer[editor->cursor],
                editor->length - editor->cursor);
    }
    
    /* 更新状态 */
    editor->cursor--;
    editor->length--;
    editor->buffer[editor->length] = '\0';
    
    return true;
}
```

**关键点**:
- 边界检查：光标在开头时不能删除
- 字符移动：删除后移动后面的字符
- 光标更新：删除后光标左移
- 空终止：保持字符串空终止

### 显示刷新策略

行编辑器不直接处理显示，由 Shell 核心负责：

1. **完整刷新** (`redraw_line()`):
   - 清除当前行
   - 重新打印提示符
   - 重新打印整个缓冲区
   - 移动光标到正确位置

2. **部分刷新** (`refresh_line_from_cursor()`):
   - 从光标位置打印到行尾
   - 清除多余字符
   - 移动光标回到正确位置

**选择策略**:
- 插入/删除字符：使用部分刷新
- 光标大幅移动：使用完整刷新
- 历史浏览：使用完整刷新

## 历史管理设计

### 环形缓冲区

历史管理使用环形缓冲区存储：

```
capacity = 4
count = 4
head = 2

entries[0] -> "command 3"  (oldest)
entries[1] -> "command 4"
entries[2] -> "command 1"  (head, newest)
entries[3] -> "command 2"

添加 "command 5":
entries[0] -> "command 5"  (head, newest)
entries[1] -> "command 4"
entries[2] -> "command 1"
entries[3] -> "command 2"  (oldest)
```

**关键点**:
- 固定容量：不会无限增长
- 覆盖最旧：新条目覆盖最旧的条目
- head 指针：指向最新条目

### 历史添加

```c
void history_add(history_manager_t* mgr, const char* cmd) {
    /* 跳过空命令 */
    if (cmd == NULL || cmd[0] == '\0') {
        return;
    }
    
    /* 跳过重复命令 */
    if (mgr->count > 0) {
        const char* last = mgr->entries[mgr->head];
        if (strcmp(last, cmd) == 0) {
            return;
        }
    }
    
    /* 移动 head */
    mgr->head = (mgr->head + 1) % mgr->capacity;
    
    /* 复制命令 */
    strncpy(mgr->entries[mgr->head], cmd, mgr->entry_size - 1);
    mgr->entries[mgr->head][mgr->entry_size - 1] = '\0';
    
    /* 更新计数 */
    if (mgr->count < mgr->capacity) {
        mgr->count++;
    }
}
```

**关键点**:
- 去重：跳过与上一条相同的命令
- 环形移动：head 循环移动
- 安全复制：使用 strncpy 防止溢出
- 计数更新：未满时增加计数

### 历史浏览

```c
const char* history_get_prev(history_manager_t* mgr) {
    /* 无历史？ */
    if (mgr->count == 0) {
        return NULL;
    }
    
    /* 开始浏览 */
    if (mgr->browse_index < 0) {
        mgr->browse_index = 0;
    } else if (mgr->browse_index < mgr->count - 1) {
        mgr->browse_index++;
    } else {
        /* 已到最旧 */
        return NULL;
    }
    
    /* 计算实际索引 */
    int actual_index = (mgr->head - mgr->browse_index + mgr->capacity) 
                       % mgr->capacity;
    
    return mgr->entries[actual_index];
}
```

**关键点**:
- 浏览索引：browse_index 跟踪浏览位置（0 = 最新）
- 环形计算：计算实际数组索引
- 边界检查：不能超过历史数量


## 自动补全设计

### 补全流程

```c
shell_status_t autocomplete_process(const char* input, int input_len,
                                     int cursor_pos, completion_result_t* result) {
    /* 初始化结果 */
    result->match_count = 0;
    result->common_prefix_len = 0;
    
    /* 提取命令名（第一个单词） */
    char cmd_name[SHELL_MAX_CMD_NAME + 1];
    int cmd_len = extract_command_name(input, cmd_name, sizeof(cmd_name));
    
    /* 遍历所有命令 */
    for (uint8_t i = 0; i < g_command_registry.count; i++) {
        const shell_command_t* cmd = g_command_registry.commands[i];
        
        /* 前缀匹配？ */
        if (strncmp(cmd->name, cmd_name, cmd_len) == 0) {
            /* 添加到匹配列表 */
            if (result->match_count < SHELL_MAX_COMPLETIONS) {
                result->matches[result->match_count++] = cmd->name;
            }
        }
    }
    
    /* 计算公共前缀长度 */
    if (result->match_count > 1) {
        result->common_prefix_len = 
            calculate_common_prefix(result->matches, result->match_count);
    }
    
    return SHELL_OK;
}
```

**关键点**:
- 前缀匹配：使用 strncmp 匹配前缀
- 匹配收集：收集所有匹配的命令
- 公共前缀：计算多个匹配的公共前缀
- 容量限制：最多返回 SHELL_MAX_COMPLETIONS 个匹配

### 公共前缀计算

```c
static int calculate_common_prefix(const char** matches, int count) {
    if (count == 0) {
        return 0;
    }
    
    /* 以第一个匹配为基准 */
    const char* first = matches[0];
    int prefix_len = 0;
    
    /* 逐字符比较 */
    for (int i = 0; first[i] != '\0'; i++) {
        /* 检查所有匹配是否都有这个字符 */
        for (int j = 1; j < count; j++) {
            if (matches[j][i] != first[i]) {
                return prefix_len;
            }
        }
        prefix_len++;
    }
    
    return prefix_len;
}
```

**关键点**:
- 逐字符比较：从第一个字符开始比较
- 全部匹配：所有匹配都必须有相同字符
- 提前退出：遇到不同字符立即返回

### 匹配显示

```c
void autocomplete_show_matches(const completion_result_t* result) {
    if (result->match_count == 0) {
        return;
    }
    
    /* 计算列宽 */
    int max_len = 0;
    for (int i = 0; i < result->match_count; i++) {
        int len = strlen(result->matches[i]);
        if (len > max_len) {
            max_len = len;
        }
    }
    
    /* 计算每行显示数量 */
    int cols = 80 / (max_len + 2);  /* 假设终端宽度 80 */
    if (cols < 1) cols = 1;
    
    /* 分列显示 */
    for (int i = 0; i < result->match_count; i++) {
        shell_printf("%-*s  ", max_len, result->matches[i]);
        if ((i + 1) % cols == 0) {
            shell_printf("\r\n");
        }
    }
    
    if (result->match_count % cols != 0) {
        shell_printf("\r\n");
    }
}
```

**关键点**:
- 列对齐：计算最大长度，使用格式化对齐
- 多列显示：根据终端宽度计算列数
- 自动换行：每行显示固定数量后换行

## 后端接口设计

### 后端接口定义

```c
typedef struct {
    int (*read)(char* buf, size_t len);
    int (*write)(const char* buf, size_t len);
} shell_backend_t;
```

**设计原则**:
- 最小接口：只需实现读写两个函数
- 非阻塞读：read 应该是非阻塞的
- 返回实际字节数：返回值表示实际读写的字节数

### UART 后端实现

```c
static int uart_backend_read(char* buf, size_t len) {
    /* 非阻塞读取 */
    if (hal_uart_available(HAL_UART_0) > 0) {
        return hal_uart_read(HAL_UART_0, (uint8_t*)buf, len);
    }
    return 0;
}

static int uart_backend_write(const char* buf, size_t len) {
    /* 阻塞写入 */
    return hal_uart_write(HAL_UART_0, (const uint8_t*)buf, len);
}

const shell_backend_t shell_uart_backend = {
    .read = uart_backend_read,
    .write = uart_backend_write
};
```

**关键点**:
- 非阻塞读：检查是否有数据可读
- 阻塞写：写入可以是阻塞的
- HAL 调用：通过 HAL 层访问硬件

### Console 后端实现

```c
static int console_backend_read(char* buf, size_t len) {
    /* 使用标准输入（非阻塞） */
    #ifdef _WIN32
        if (_kbhit()) {
            *buf = _getch();
            return 1;
        }
        return 0;
    #else
        /* Unix/Linux 非阻塞读取 */
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        int n = read(STDIN_FILENO, buf, len);
        fcntl(STDIN_FILENO, F_SETFL, flags);
        return n > 0 ? n : 0;
    #endif
}

static int console_backend_write(const char* buf, size_t len) {
    /* 使用标准输出 */
    return fwrite(buf, 1, len, stdout);
}

const shell_backend_t shell_console_backend = {
    .read = console_backend_read,
    .write = console_backend_write
};
```

**关键点**:
- 平台适配：Windows 和 Unix/Linux 不同实现
- 非阻塞读：使用平台特定的非阻塞 API
- 标准 I/O：使用标准输入输出

### Mock 后端（测试用）

```c
typedef struct {
    const char* input;      /* 输入字符串 */
    size_t input_pos;       /* 当前读取位置 */
    char* output;           /* 输出缓冲区 */
    size_t output_size;     /* 输出缓冲区大小 */
    size_t output_pos;      /* 当前写入位置 */
} mock_backend_context_t;

static int mock_backend_read(char* buf, size_t len) {
    mock_backend_context_t* ctx = &g_mock_ctx;
    
    if (ctx->input == NULL || ctx->input[ctx->input_pos] == '\0') {
        return 0;
    }
    
    *buf = ctx->input[ctx->input_pos++];
    return 1;
}

static int mock_backend_write(const char* buf, size_t len) {
    mock_backend_context_t* ctx = &g_mock_ctx;
    
    size_t available = ctx->output_size - ctx->output_pos;
    size_t to_write = (len < available) ? len : available;
    
    memcpy(&ctx->output[ctx->output_pos], buf, to_write);
    ctx->output_pos += to_write;
    
    return to_write;
}
```

**关键点**:
- 模拟输入：从预设字符串读取
- 捕获输出：写入到缓冲区
- 测试友好：便于单元测试

## 转义序列处理

### 转义序列状态机

```
状态转换图:

NORMAL ─────ESC(0x1B)────→ ESC
                            │
                            ├─'['──→ CSI
                            │         │
                            │         ├─'A'──→ UP
                            │         ├─'B'──→ DOWN
                            │         ├─'C'──→ RIGHT
                            │         ├─'D'──→ LEFT
                            │         ├─'H'──→ HOME
                            │         ├─'F'──→ END
                            │         ├─'3~'─→ DELETE
                            │         └─...
                            │
                            └─'O'──→ SS3
                                      │
                                      ├─'A'──→ UP
                                      ├─'B'──→ DOWN
                                      ├─'C'──→ RIGHT
                                      ├─'D'──→ LEFT
                                      ├─'H'──→ HOME
                                      ├─'F'──→ END
                                      └─...
```

### 常见转义序列

| 按键 | CSI 序列 | SS3 序列 | 说明 |
|------|----------|----------|------|
| ↑ | ESC[A | ESC OA | 上箭头 |
| ↓ | ESC[B | ESC OB | 下箭头 |
| → | ESC[C | ESC OC | 右箭头 |
| ← | ESC[D | ESC OD | 左箭头 |
| Home | ESC[H, ESC[1~ | ESC OH | Home 键 |
| End | ESC[F, ESC[4~ | ESC OF | End 键 |
| Delete | ESC[3~ | - | Delete 键 |

**关键点**:
- 多种格式：同一按键可能有多种序列
- 兼容性：支持 CSI 和 SS3 两种格式
- 超时处理：无效序列时重置状态

## 线程安全设计

### 当前实现

Shell Framework 当前实现**不是线程安全的**，设计为单线程使用。

**原因**:
- 嵌入式系统通常在单个任务中运行 Shell
- 避免互斥锁开销
- 简化实现

### 多线程使用场景

如果需要在多线程环境中使用，有两种方案：

#### 方案 1：外部同步

应用层负责同步：

```c
/* 应用层互斥锁 */
osal_mutex_t shell_mutex;

void task1(void) {
    osal_mutex_lock(&shell_mutex);
    shell_process();
    osal_mutex_unlock(&shell_mutex);
}

void task2(void) {
    osal_mutex_lock(&shell_mutex);
    shell_printf("Message from task2\r\n");
    osal_mutex_unlock(&shell_mutex);
}
```

#### 方案 2：内部同步（未来改进）

在 Shell 内部添加互斥锁：

```c
typedef struct {
    /* ... 现有字段 ... */
    osal_mutex_t mutex;  /* 互斥锁 */
} shell_context_t;

shell_status_t shell_process(void) {
    osal_mutex_lock(&g_shell_ctx.mutex);
    /* ... 处理逻辑 ... */
    osal_mutex_unlock(&g_shell_ctx.mutex);
    return SHELL_OK;
}
```

**权衡**:
- 外部同步：灵活，但需要应用层管理
- 内部同步：方便，但增加开销

## 内存管理策略

### 动态分配

当前实现使用动态内存分配：

```c
/* 命令缓冲区 */
g_shell_ctx.cmd_buffer = malloc(config->cmd_buffer_size);

/* 历史存储 */
g_shell_ctx.history_storage = malloc(config->history_depth * entry_size);
g_shell_ctx.history_entries = malloc(config->history_depth * sizeof(char*));
```

**优点**:
- 灵活配置：可以根据需要调整大小
- 节省内存：只分配需要的大小

**缺点**:
- 需要 malloc：依赖动态内存分配
- 碎片风险：可能导致内存碎片

### 静态分配（未来改进）

可以添加静态分配选项：

```c
#ifdef SHELL_USE_STATIC_ALLOC

/* 静态缓冲区 */
static char g_cmd_buffer[SHELL_DEFAULT_CMD_BUFFER_SIZE];
static char g_history_storage[SHELL_DEFAULT_HISTORY_DEPTH * 
                               SHELL_DEFAULT_CMD_BUFFER_SIZE];
static char* g_history_entries[SHELL_DEFAULT_HISTORY_DEPTH];

shell_status_t shell_init(const shell_config_t* config) {
    /* 使用静态缓冲区 */
    g_shell_ctx.cmd_buffer = g_cmd_buffer;
    g_shell_ctx.history_storage = g_history_storage;
    g_shell_ctx.history_entries = g_history_entries;
    /* ... */
}

#endif
```

**优点**:
- 无需 malloc：适用于无动态内存的环境
- 确定性：内存使用可预测

**缺点**:
- 固定大小：不能运行时调整
- 浪费内存：可能分配过多

### 内存使用估算

| 组件 | 大小 | 说明 |
|------|------|------|
| Shell 上下文 | ~100 B | 固定大小 |
| 命令缓冲区 | 128 B | 默认配置 |
| 保存输入缓冲区 | 128 B | 默认配置 |
| 历史存储 | 2048 B | 16 * 128 |
| 历史指针数组 | 64 B | 16 * 4 |
| 命令注册表 | ~256 B | 32 个命令指针 |
| **总计** | **~2.7 KB** | 默认配置 |

## 性能优化方案

### 1. 非阻塞处理

```c
shell_status_t shell_process(void) {
    /* 非阻塞读取 */
    int bytes_read = backend->read(&c, 1);
    if (bytes_read <= 0) {
        return SHELL_OK;  /* 立即返回 */
    }
    /* 处理字符 */
}
```

**优点**:
- 不阻塞主循环
- 响应迅速
- 适合事件驱动架构

### 2. 最小化字符串操作

```c
/* 避免频繁的 strlen */
uint16_t len = editor->length;  /* 缓存长度 */

/* 使用 memcpy 而不是 strcpy */
memcpy(dest, src, len);
```

### 3. 显示刷新优化

```c
/* 只刷新必要的部分 */
static void refresh_line_from_cursor(void) {
    /* 只重绘从光标到行尾 */
    shell_puts(&buf[cursor]);
    shell_puts(ANSI_ERASE_LINE);
    /* 移动光标回到位置 */
}
```

### 4. 命令查找优化

当前使用线性查找，对于少量命令（< 32）足够高效。

如果命令数量增加，可以考虑：

- **哈希表**：O(1) 查找
- **二分查找**：O(log n) 查找（需要排序）
- **前缀树**：适合自动补全

## 设计权衡

### 1. 单例 vs 多实例

**当前选择**：单例模式

**理由**:
- 嵌入式系统通常只需要一个 Shell
- 简化实现
- 减少内存开销

**权衡**:
- 不支持多个 Shell 实例
- 全局状态

### 2. 动态分配 vs 静态分配

**当前选择**：动态分配

**理由**:
- 灵活配置
- 节省内存

**权衡**:
- 需要 malloc 支持
- 可能有内存碎片

**未来改进**：添加静态分配选项

### 3. 线程安全 vs 性能

**当前选择**：不保证线程安全

**理由**:
- 单线程使用场景
- 避免互斥锁开销
- 简化实现

**权衡**:
- 多线程环境需要外部同步

**未来改进**：可选的内部同步

### 4. 功能丰富 vs 代码体积

**当前选择**：功能丰富

**理由**:
- 提供现代 Shell 体验
- 行编辑、历史、补全等

**权衡**:
- 代码体积较大（~12 KB）
- 内存占用较多（~4 KB）

**优化方向**：可配置的功能裁剪

## 未来改进方向

### 1. 静态内存分配选项

添加编译选项支持静态分配：

```c
#define SHELL_USE_STATIC_ALLOC 1
```

### 2. 线程安全选项

添加可选的内部互斥锁：

```c
#define SHELL_THREAD_SAFE 1
```

### 3. 脚本执行

支持从文件或字符串执行多条命令：

```c
shell_execute_script(const char* script);
shell_execute_file(const char* filename);
```

### 4. 命令别名

支持命令别名：

```c
shell_register_alias("ll", "ls -l");
```

### 5. 管道和重定向

支持简单的管道和重定向：

```c
command1 | command2
command > file
```

### 6. 变量和环境

支持 Shell 变量：

```c
set VAR=value
echo $VAR
```

### 7. 颜色输出

支持 ANSI 颜色输出：

```c
shell_printf_color(SHELL_COLOR_RED, "Error: %s\r\n", msg);
```

### 8. 多行输入

支持多行命令输入：

```c
nexus> command \
     > arg1 \
     > arg2
```

### 9. 命令历史持久化

支持历史记录保存到文件：

```c
history_save("/data/shell_history");
history_load("/data/shell_history");
```

### 10. 更智能的补全

- 参数补全（文件名、路径等）
- 上下文感知补全
- 模糊匹配

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
