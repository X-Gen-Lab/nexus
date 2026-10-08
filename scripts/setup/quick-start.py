#!/usr/bin/env python3
"""
Nexus 快速开始脚本
一键设置完整的开发环境并运行第一个示例

使用方法:
    python quick-start.py [选项]

选项:
    --platform, -p    目标平台: native, stm32f4 (默认: native)
    --skip-deps       跳过依赖安装 (假设已安装)
    --help, -h        显示帮助信息
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


def print_banner():
    """打印欢迎横幅"""
    banner = """
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║                    🚀 Nexus 快速开始                         ║
║                                                              ║
║              欢迎使用 Nexus 嵌入式开发平台!                    ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
"""
    print(banner)


def run_setup(platform):
    """运行环境搭建脚本"""
    print("🔧 正在设置开发环境...")
    
    setup_script = Path(__file__).parent / "setup.py"
    cmd = [
        sys.executable, str(setup_script),
        "--platform", platform,
        "--dev",
        "--test"
    ]
    
    result = subprocess.run(cmd)
    return result.returncode == 0


def build_project(platform):
    """构建项目"""
    print("🔨 正在构建项目...")
    
    build_script = Path(__file__).parent.parent / "building" / "build.py"
    cmd = [
        sys.executable, str(build_script),
        "--platform", platform,
        "--type", "release"
    ]
    
    result = subprocess.run(cmd)
    return result.returncode == 0


def run_tests():
    """运行测试"""
    print("🧪 正在运行测试...")
    
    test_script = Path(__file__).parent.parent / "test" / "test.py"
    cmd = [sys.executable, str(test_script)]
    
    result = subprocess.run(cmd)
    return result.returncode == 0


def show_next_steps(platform):
    """显示后续步骤"""
    print("\n" + "="*60)
    print("🎉 恭喜! Nexus 开发环境已准备就绪!")
    print("="*60)
    
    print("\n📁 项目结构:")
    print("  nexus/")
    print("  ├── hal/              # 硬件抽象层")
    print("  ├── osal/             # 操作系统抽象层")
    print("  ├── platforms/        # 平台特定代码")
    print("  ├── applications/     # 示例应用")
    print("  └── tests/            # 单元测试")
    
    print("\n🛠️ 常用命令:")
    print("  # 构建项目")
    print("  python scripts/building/build.py --preset linux-gcc-debug --stage all")
    print("")
    print("  # 运行测试")
    print("  python scripts/test/test.py")
    print("")
    print("  # 格式化代码")
    print("  python scripts/tools/format.py")
    print("")
    print("  # 生成文档")
    print("  python scripts/tools/docs.py")
    
    if platform == "stm32f4":
        print("\n🔌 STM32F4 开发:")
        print("  # 构建 STM32F4 固件")
        print("  python scripts/building/build.py --preset stm32-armgcc-debug --stage build")
        print("")
        print("  # 输出文件位置:")
        print("  build-stm32f4/applications/blinky/blinky.elf")
        print("  build-stm32f4/applications/blinky/blinky.bin")
        print("  build-stm32f4/applications/blinky/blinky.hex")
    
    print("\n📚 文档:")
    print("  - API 文档: docs/api/html/index.html")
    print("  - 用户指南: docs/sphinx/_build/html/index.html")
    print("  - 贡献指南: CONTRIBUTING.md")
    
    print("\n💡 提示:")
    print("  - 使用 VS Code 打开项目获得最佳开发体验")
    print("  - 代码会在保存时自动格式化")
    print("  - 运行测试确保代码质量")
    
    print("\n🆘 需要帮助?")
    print("  - 查看 README.md")
    print("  - 访问项目文档")
    print("  - 提交 Issue 到 GitHub")


def create_hello_world():
    """创建一个简单的 Hello World 示例"""
    print("📝 创建 Hello World 示例...")
    
    hello_dir = Path("applications/hello")
    hello_dir.mkdir(parents=True, exist_ok=True)
    
    # 创建 CMakeLists.txt
    cmake_content = """# Hello World Application
add_executable(hello
    src/main.c
)

target_link_libraries(hello
    nexus_hal
    nexus_osal
)

target_include_directories(hello PRIVATE
    include
)
"""
    
    with open(hello_dir / "CMakeLists.txt", "w") as f:
        f.write(cmake_content)
    
    # 创建源码目录
    src_dir = hello_dir / "src"
    src_dir.mkdir(exist_ok=True)
    
    # 创建 main.c
    main_content = """/**
 * \\file            main.c
 * \\brief           Hello World Application
 */

#include "hal/hal_system.h"
#include "hal/hal_gpio.h"
#include "osal/osal.h"

#include <stdio.h>

int main(void)
{
    // 初始化系统
    hal_status_t status = hal_system_init();
    if (status != HAL_OK) {
        printf("系统初始化失败: %d\\n", status);
        return -1;
    }
    
    printf("Hello, Nexus World!\\n");
    printf("欢迎使用 Nexus 嵌入式开发平台\\n");
    
    // 配置 LED (如果是嵌入式平台)
#ifndef NEXUS_PLATFORM_NATIVE
    hal_gpio_config_t led_config = {
        .direction   = HAL_GPIO_DIR_OUTPUT,
        .pull        = HAL_GPIO_PULL_NONE,
        .output_mode = HAL_GPIO_OUTPUT_PP,
        .speed       = HAL_GPIO_SPEED_LOW,
        .init_level  = HAL_GPIO_LEVEL_LOW
    };
    
    hal_gpio_init(HAL_GPIO_PORT_A, 5, &led_config);
    
    // LED 闪烁循环
    for (int i = 0; i < 10; i++) {
        hal_gpio_toggle(HAL_GPIO_PORT_A, 5);
        hal_delay_ms(500);
        printf("LED 闪烁 %d/10\\n", i + 1);
    }
#endif
    
    printf("Hello World 示例运行完成!\\n");
    return 0;
}
"""
    
    with open(src_dir / "main.c", "w", encoding="utf-8") as f:
        f.write(main_content)
    
    print("✓ Hello World 示例创建完成")


def main():
    parser = argparse.ArgumentParser(description="Nexus 快速开始脚本")
    parser.add_argument("-p", "--platform", 
                        choices=["native", "stm32f4"],
                        default="native", 
                        help="目标平台")
    parser.add_argument("--skip-deps", action="store_true",
                        help="跳过依赖安装")
    
    args = parser.parse_args()
    
    print_banner()
    
    print(f"🎯 目标平台: {args.platform}")
    print(f"📦 跳过依赖安装: {'是' if args.skip_deps else '否'}")
    
    try:
        # 1. 安装依赖 (如果需要)
        if not args.skip_deps:
            if not run_setup(args.platform):
                print("❌ 环境搭建失败")
                return 1
        
        # 2. 创建示例
        create_hello_world()
        
        # 3. 构建项目
        if not build_project(args.platform):
            print("❌ 项目构建失败")
            return 1
        
        # 4. 运行测试 (仅 native 平台)
        if args.platform == "native":
            if not run_tests():
                print("⚠️ 测试失败，但环境搭建完成")
        
        # 5. 显示后续步骤
        show_next_steps(args.platform)
        
        return 0
        
    except KeyboardInterrupt:
        print("\n\n❌ 用户中断操作")
        return 1
    except Exception as e:
        print(f"\n\n❌ 发生错误: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())