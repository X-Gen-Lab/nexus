# Nexus Scripts

本目录包含 Nexus 项目的常用脚本，支持 Windows (.bat)、Unix (.sh) 和跨平台 Python (.py) 脚本。

## 目录结构

```
scripts/
├── README.md               # 本文档
├── building/               # 构建相关脚本
│   ├── build.bat           # Windows 构建
│   ├── build.sh            # Unix 构建
│   └── build.py            # Python 跨平台构建
├── test/                   # 测试相关脚本
│   ├── test.bat            # Windows 测试
│   ├── test.sh             # Unix 测试
│   └── test.py             # Python 跨平台测试
├── tools/                  # 开发工具脚本
│   ├── format.bat          # Windows 格式化
│   ├── format.sh           # Unix 格式化
│   ├── format.py           # Python 跨平台格式化
│   ├── clean.bat           # Windows 清理
│   ├── clean.sh            # Unix 清理
│   ├── clean.py            # Python 跨平台清理
│   ├── docs.bat            # Windows 文档生成
│   ├── docs.sh             # Unix 文档生成
│   └── docs.py             # Python 跨平台文档生成
└── ci/                     # CI/CD 脚本
    └── ci_build.py         # CI 构建脚本
```

## 使用方法

### Python 脚本 (推荐，跨平台)

```bash
# 构建
python scripts/building/build.py                 # Debug 构建
python scripts/building/build.py -t release      # Release 构建
python scripts/building/build.py -c              # 清理后构建
python scripts/building/build.py -j 8            # 指定并行数

# 测试
python scripts/test/test.py                      # 运行所有测试
python scripts/test/test.py -f "HalGpioTest.*"   # 过滤测试
python scripts/test/test.py -v                   # 详细输出
python scripts/test/test.py --xml report.xml     # 生成 XML 报告

# 格式化
python scripts/tools/format.py                   # 格式化代码
python scripts/tools/format.py -c                # 仅检查格式

# 清理
python scripts/tools/clean.py                    # 清理构建目录
python scripts/tools/clean.py -a                 # 清理所有

# 文档
python scripts/tools/docs.py                     # 生成所有文档
python scripts/tools/docs.py -t doxygen          # 仅 Doxygen

# CI
python scripts/ci/ci_build.py                    # 运行完整 CI
python scripts/ci/ci_build.py --stage build      # 仅构建阶段
python scripts/ci/ci_build.py --stage test       # 仅测试阶段
```

### Unix (Linux/macOS)

```bash
./scripts/building/build.sh                      # 构建
./scripts/building/build.sh release              # Release 构建
./scripts/test/test.sh                           # 测试
./scripts/tools/format.sh                        # 格式化
./scripts/tools/clean.sh                         # 清理
./scripts/tools/docs.sh                          # 文档
```

### Windows

```cmd
scripts\building\build.bat                       REM 构建
scripts\building\build.bat release               REM Release 构建
scripts\test\test.bat                            REM 测试
scripts\tools\format.bat                         REM 格式化
scripts\tools\clean.bat                          REM 清理
scripts\tools\docs.bat                           REM 文档
```

## 依赖

- Python 3.7+ (Python 脚本)
- CMake 3.16+
- C/C++ 编译器 (GCC, Clang, MSVC)
- clang-format (代码格式化)
- Doxygen (API 文档)
- Sphinx (用户文档)
