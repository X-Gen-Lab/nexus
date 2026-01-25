# Config Manager 架构设计文档

## 1. 概述

Config Manager 是 Nexus 嵌入式平台的配置管理框架，提供灵活、安全、持久化的键值配置存储能力。

### 1.1 设计目标

- **灵活性**: 支持多种数据类型和存储后端
- **安全性**: 提供加密存储和访问控制
- **可靠性**: 确保配置数据的持久化和一致性
- **易用性**: 简洁的 API 设计，降低使用门槛
- **资源可控**: 支持静态内存分配，适用于资源受限环境
- **可扩展性**: 模块化设计，易于添加新功能

### 1.2 核心特性

- 支持 7 种数据类型（int32/64, uint32, float, bool, string, blob）
- 命名空间隔离机制
- 默认值管理系统
- 配置变更通知回调
- 可插拔存储后端（RAM、Flash、自定义）
- 导入/导出功能（JSON、二进制格式）
- AES-128/256 加密支持
- 线程安全保护

## 2. 系统架构

### 2.1 分层架构

```
┌─────────────────────────────────────────────────────────────┐
│                      Application Layer                       │
│                    (User Application Code)                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                       Public API Layer                       │
│  config_set_*() / config_get_*() / config_ns_*() / ...      │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Core Logic Layer                        │
├──────────────┬──────────────┬──────────────┬────────────────┤
│ Config Store │  Namespace   │   Callback   │    Default     │
│   Manager    │   Manager    │   Manager    │    Manager     │
└──────────────┴──────────────┴──────────────┴────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Auxiliary Services                        │
├──────────────┬──────────────┬──────────────┬────────────────┤
│    Crypto    │    Export    │    Import    │     Query      │
│   Service    │   Service    │   Service    │    Service     │
└──────────────┴──────────────┴──────────────┴────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Backend Abstraction                       │
│              (Backend Interface + Adapters)                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Storage Backends                          │
├──────────────┬──────────────┬──────────────┬────────────────┤
│ RAM Backend  │Flash Backend │ Mock Backend │Custom Backend  │
└──────────────┴──────────────┴──────────────┴────────────────┘
```

### 2.2 模块职责

#### 2.2.1 Config Store Manager
- 管理配置键值对的内存存储
- 提供基本的 CRUD 操作
- 维护配置项的元数据（类型、大小、标志）
- 实现哈希表或线性查找机制

#### 2.2.2 Namespace Manager
- 管理命名空间的创建和销毁
- 维护命名空间到 ID 的映射
- 提供命名空间隔离功能
- 支持命名空间级别的批量操作

#### 2.2.3 Callback Manager
- 注册和管理配置变更回调
- 支持特定键和通配符回调
- 在配置变更时触发相应回调
- 管理回调的生命周期

#### 2.2.4 Default Manager
- 存储和管理默认值
- 提供默认值查询接口
- 支持批量注册默认值
- 实现配置重置功能

#### 2.2.5 Crypto Service
- 提供 AES-128/256 加密/解密
- 管理加密密钥
- 支持密钥轮换
- 处理加密数据的存储格式

#### 2.2.6 Export/Import Service
- 实现 JSON 格式导出/导入
- 实现二进制格式导出/导入
- 支持命名空间级别的导出
- 处理格式转换和验证

#### 2.2.7 Query Service
- 提供配置查询接口
- 实现配置遍历功能
- 支持条件过滤
- 提供统计信息

#### 2.2.8 Backend Abstraction
- 定义统一的后端接口
- 管理后端的注册和切换
- 协调后端操作
- 处理后端错误

## 3. 核心数据结构

### 3.1 配置项结构

```c
typedef struct {
    char key[CONFIG_MAX_KEY_LEN];     /* 配置键 */
    config_type_t type;                /* 数据类型 */
    uint16_t value_size;               /* 值大小 */
    uint8_t flags;                     /* 标志位 */
    uint8_t namespace_id;              /* 命名空间 ID */
    uint8_t value[CONFIG_MAX_VALUE_SIZE]; /* 值数据 */
} config_entry_t;
```

### 3.2 命名空间结构

```c
typedef struct {
    char name[CONFIG_MAX_NS_NAME_LEN]; /* 命名空间名称 */
    uint8_t id;                        /* 命名空间 ID */
    bool active;                       /* 是否激活 */
    uint16_t ref_count;                /* 引用计数 */
} config_namespace_entry_t;
```

### 3.3 回调结构

```c
typedef struct {
    char key[CONFIG_MAX_KEY_LEN];     /* 监听的键（空表示通配符）*/
    config_change_cb_t callback;       /* 回调函数 */
    void* user_data;                   /* 用户数据 */
    bool active;                       /* 是否激活 */
} config_callback_entry_t;
```

### 3.4 默认值结构

```c
typedef struct {
    char key[CONFIG_MAX_KEY_LEN];     /* 配置键 */
    config_type_t type;                /* 数据类型 */
    uint16_t value_size;               /* 值大小 */
    uint8_t value[CONFIG_MAX_VALUE_SIZE]; /* 默认值数据 */
} config_default_entry_t;
```

## 4. 关键流程

### 4.1 初始化流程

```
config_init()
    │
    ├─> 验证配置参数
    │
    ├─> 初始化 Config Store
    │   └─> 分配内存池
    │
    ├─> 初始化 Namespace Manager
    │   └─> 创建默认命名空间
    │
    ├─> 初始化 Callback Manager
    │   └─> 分配回调数组
    │
    ├─> 初始化 Default Manager
    │   └─> 分配默认值数组
    │
    ├─> 初始化 Crypto Service（可选）
    │   └─> 准备加密上下文
    │
    └─> 设置初始化标志
```

### 4.2 配置设置流程

```
config_set_i32(key, value)
    │
    ├─> 检查初始化状态
    │
    ├─> 验证参数（键长度、值范围）
    │
    ├─> 解析命名空间（如果键包含命名空间前缀）
    │
    ├─> 查询旧值（用于回调）
    │
    ├─> 存储新值到 Config Store
    │   ├─> 查找现有条目
    │   ├─> 更新或创建条目
    │   └─> 设置类型和标志
    │
    ├─> 触发变更回调
    │   ├─> 查找特定键回调
    │   ├─> 查找通配符回调
    │   └─> 依次调用回调函数
    │
    ├─> 自动提交（如果启用）
    │   └─> 调用后端 write()
    │
    └─> 返回状态
```

### 4.3 配置读取流程

```
config_get_i32(key, value, default_val)
    │
    ├─> 检查初始化状态
    │
    ├─> 验证参数
    │
    ├─> 解析命名空间
    │
    ├─> 从 Config Store 查询
    │   ├─> 查找条目
    │   └─> 验证类型匹配
    │
    ├─> 如果找到
    │   ├─> 检查是否加密
    │   ├─> 解密（如果需要）
    │   └─> 返回值
    │
    └─> 如果未找到
        ├─> 查询默认值
        ├─> 如果有默认值，返回默认值
        └─> 否则返回用户提供的默认值
```

### 4.4 持久化流程

```
config_commit()
    │
    ├─> 检查后端是否设置
    │
    ├─> 遍历所有配置项
    │   │
    │   └─> 对每个标记为持久化的项
    │       ├─> 序列化键值对
    │       ├─> 加密（如果需要）
    │       └─> 调用后端 write()
    │
    ├─> 调用后端 commit()（如果支持）
    │
    └─> 返回状态
```

### 4.5 加载流程

```
config_load()
    │
    ├─> 检查后端是否设置
    │
    ├─> 调用后端枚举接口（或预定义键列表）
    │
    ├─> 对每个键
    │   ├─> 调用后端 read()
    │   ├─> 解密（如果需要）
    │   ├─> 反序列化
    │   └─> 存储到 Config Store
    │
    └─> 返回状态
```

## 5. 存储后端接口

### 5.1 后端接口定义

```c
struct config_backend {
    const char* name;                      /* 后端名称 */
    config_backend_init_fn init;           /* 初始化 */
    config_backend_deinit_fn deinit;       /* 反初始化 */
    config_backend_read_fn read;           /* 读取 */
    config_backend_write_fn write;         /* 写入 */
    config_backend_erase_fn erase;         /* 删除 */
    config_backend_erase_all_fn erase_all; /* 删除全部 */
    config_backend_commit_fn commit;       /* 提交 */
    void* ctx;                             /* 上下文 */
};
```

### 5.2 后端实现要求

#### 必需接口
- `read()`: 根据键读取数据
- `write()`: 根据键写入数据
- `erase()`: 根据键删除数据

#### 可选接口
- `init()`: 后端初始化（如打开文件、初始化 Flash）
- `deinit()`: 后端清理
- `erase_all()`: 批量删除（性能优化）
- `commit()`: 提交事务（用于支持事务的后端）

### 5.3 内置后端

#### RAM Backend
- **用途**: 测试、临时存储
- **特点**: 快速、易失性
- **实现**: 使用内存数组或哈希表

#### Flash Backend
- **用途**: 生产环境持久化存储
- **特点**: 非易失、磨损均衡
- **实现**: 基于 HAL Flash 接口，使用键值对格式

#### Mock Backend
- **用途**: 单元测试
- **特点**: 可控制行为、记录操作
- **实现**: 内存存储 + 操作日志

## 6. 线程安全设计

### 6.1 保护策略

使用全局互斥锁保护所有公共 API：

```c
static osal_mutex_t g_config_mutex;

config_status_t config_set_i32(const char* key, int32_t value) {
    osal_mutex_lock(&g_config_mutex);
    
    /* 执行操作 */
    config_status_t status = config_store_set(...);
    
    osal_mutex_unlock(&g_config_mutex);
    return status;
}
```

### 6.2 死锁避免

- 回调函数中不允许调用 Config API
- 后端接口实现不应持有其他锁
- 使用超时机制防止永久阻塞

### 6.3 性能考虑

- 读多写少场景可考虑读写锁
- 命名空间级别的细粒度锁
- 无锁数据结构（高级优化）

## 7. 内存管理

### 7.1 静态分配策略

所有内存在初始化时静态分配：

```c
static config_entry_t g_config_entries[CONFIG_MAX_KEYS];
static config_namespace_entry_t g_namespaces[CONFIG_MAX_NAMESPACES];
static config_callback_entry_t g_callbacks[CONFIG_MAX_CALLBACKS];
static config_default_entry_t g_defaults[CONFIG_MAX_DEFAULTS];
```

### 7.2 内存占用估算

```
总内存 = 配置项内存 + 命名空间内存 + 回调内存 + 默认值内存

配置项内存 = max_keys × (max_key_len + max_value_size + 元数据)
命名空间内存 = max_namespaces × (名称长度 + 元数据)
回调内存 = max_callbacks × (键长度 + 函数指针 + 用户数据指针)
默认值内存 = max_defaults × (键长度 + 值大小)
```

示例（默认配置）：
```
配置项: 64 × (32 + 256 + 8) = 18,944 字节
命名空间: 8 × (16 + 4) = 160 字节
回调: 16 × (32 + 8) = 640 字节
默认值: 32 × (32 + 256) = 9,216 字节
总计: ~29 KB
```

### 7.3 内存优化建议

- 根据实际需求调整 `max_keys` 和 `max_value_size`
- 使用命名空间减少键名长度
- 对不常用功能禁用相关模块
- 考虑使用压缩算法（高级）

## 8. 加密设计

### 8.1 加密架构

```
明文数据 → AES 加密 → 添加元数据 → 存储
存储数据 → 解析元数据 → AES 解密 → 明文数据
```

### 8.2 加密元数据格式

```
[1 byte: 算法标识][1 byte: IV 长度][N bytes: IV][加密数据]
```

### 8.3 密钥管理

- 密钥存储在内存中（运行时设置）
- 支持密钥轮换（重新加密所有加密项）
- 建议使用硬件安全模块（HSM）存储密钥

### 8.4 安全考虑

- 使用随机 IV（初始化向量）
- 定期轮换密钥
- 敏感数据使用加密存储
- 防止侧信道攻击（时间攻击）

## 9. 错误处理

### 9.1 错误码设计

所有 API 返回 `config_status_t` 枚举类型：

```c
typedef enum {
    CONFIG_OK = 0,                    /* 成功 */
    CONFIG_ERROR = 1,                 /* 通用错误 */
    CONFIG_ERROR_INVALID_PARAM = 2,   /* 参数错误 */
    /* ... 更多错误码 ... */
} config_status_t;
```

### 9.2 错误传播

- 底层错误向上传播
- 保留最后错误码（`g_last_error`）
- 提供错误码到字符串转换

### 9.3 错误恢复

- 参数错误：返回错误，不改变状态
- 内存不足：返回错误，可能部分成功
- 后端错误：返回错误，保持内存状态一致

## 10. 性能优化

### 10.1 查找优化

- 使用哈希表加速键查找（O(1) vs O(n)）
- 缓存最近访问的配置项
- 命名空间前缀树（Trie）

### 10.2 存储优化

- 批量提交减少 Flash 写入次数
- 延迟写入（定时或达到阈值）
- 增量更新（只写变更项）

### 10.3 回调优化

- 回调列表按使用频率排序
- 支持回调优先级
- 异步回调（可选）

## 11. 扩展性设计

### 11.1 插件机制

- 自定义后端插件
- 自定义加密算法插件
- 自定义序列化格式插件

### 11.2 功能模块化

通过编译选项启用/禁用功能：

```c
#define CONFIG_ENABLE_ENCRYPTION  1
#define CONFIG_ENABLE_JSON        1
#define CONFIG_ENABLE_CALLBACKS   1
#define CONFIG_ENABLE_DEFAULTS    1
```

### 11.3 版本兼容性

- 导出数据包含版本信息
- 支持旧版本数据迁移
- API 向后兼容

## 12. 设计权衡

### 12.1 静态 vs 动态内存

**选择**: 静态内存分配

**理由**:
- 嵌入式环境内存受限
- 避免内存碎片
- 可预测的内存占用
- 更高的可靠性

**代价**:
- 灵活性降低
- 可能浪费内存

### 12.2 同步 vs 异步 API

**选择**: 同步 API

**理由**:
- 简化实现和使用
- 配置操作通常不频繁
- 避免复杂的状态管理

**代价**:
- 可能阻塞调用线程
- Flash 写入时延迟较高

### 12.3 类型安全 vs 灵活性

**选择**: 强类型 API

**理由**:
- 编译时类型检查
- 减少运行时错误
- 更清晰的接口

**代价**:
- API 数量增加
- 代码量增加

### 12.4 功能完整性 vs 代码大小

**选择**: 模块化设计，可选功能

**理由**:
- 满足不同应用需求
- 控制代码大小
- 降低资源占用

**实现**:
- 通过宏开关控制功能
- 链接时优化（LTO）

## 13. 未来改进方向

### 13.1 短期改进

- 添加配置项访问权限控制
- 支持配置项的只读属性
- 实现配置变更历史记录
- 添加配置项的元数据（描述、单位等）

### 13.2 中期改进

- 支持配置项的范围验证
- 实现配置模板和继承
- 添加配置项的依赖关系管理
- 支持远程配置同步

### 13.3 长期改进

- 分布式配置管理
- 配置热更新机制
- 配置版本控制和回滚
- 图形化配置工具

## 14. 参考资料

- ESP-IDF NVS (Non-Volatile Storage) 设计
- Linux Kernel Configuration System
- Android SharedPreferences 实现
- Embedded Systems Design Patterns
