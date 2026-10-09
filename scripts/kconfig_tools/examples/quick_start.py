#!/usr/bin/env python3
"""Interactive entry point for scaffold examples and strict product validation."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
EXAMPLES = Path(__file__).resolve().parent
PERIPHERALS = ("UART", "GPIO", "SPI", "I2C", "ADC", "DAC", "CRC", "WATCHDOG")
CONFIG_FILES = ("batch_config.yaml", "stm32_config.yaml", "minimal_config.yaml", "custom_peripheral.json")


def select(items, prompt):
    for index, item in enumerate(items, 1):
        print(f"  {index}. {item}")
    try:
        value = int(input(prompt).strip())
    except ValueError:
        print("请输入菜单范围内的整数。")
        return None
    if value == 0:
        return None
    if not 1 <= value <= len(items):
        print("选择超出范围。")
        return None
    return items[value - 1]


def run_script(script, *arguments):
    command = [sys.executable, str(script), *map(str, arguments)]
    print("执行:", command)
    try:
        return subprocess.run(command, cwd=ROOT, check=False).returncode
    except OSError as error:
        print(f"执行失败: {error}", file=sys.stderr)
        return 1


def platform_name():
    value = input("平台符号 (NATIVE/STM32): ").strip().upper()
    if not re.fullmatch(r"[A-Z][A-Z0-9_]*", value):
        raise ValueError("平台必须是有效的 Kconfig 标识符")
    return value


def generate_single():
    peripheral = select(PERIPHERALS, "外设序号 (0 返回): ")
    if peripheral is None:
        return 0
    platform = platform_name()
    value = input("实例数 (默认 1): ").strip()
    try:
        instances = int(value) if value else 1
    except ValueError as error:
        raise ValueError("实例数必须是正整数") from error
    if instances < 1:
        raise ValueError("实例数必须是正整数")
    output = input("输出目录 (默认 build/kconfig-examples/single): ").strip() or "build/kconfig-examples/single"
    return run_script(EXAMPLES / "generate_all_peripherals.py", platform, output,
                      "--peripheral", peripheral, "--instances", instances)


def generate_all():
    platform = platform_name()
    output = input("输出目录 (默认 build/kconfig-examples/all): ").strip() or "build/kconfig-examples/all"
    return run_script(EXAMPLES / "generate_all_peripherals.py", platform, output)


def validate_names():
    path = input("待检查的 scaffold 文件或目录: ").strip()
    if not path:
        raise ValueError("路径不能为空")
    return run_script(EXAMPLES / "validate_project.py", Path(path).resolve())


def validate_product():
    path = input("产品配置 fragment 路径: ").strip()
    if not path:
        raise ValueError("fragment 路径不能为空")
    return run_script(ROOT / "scripts/nexus_config.py", "validate", "--config", Path(path).resolve())


def show_examples():
    name = select(CONFIG_FILES, "文件序号 (0 返回): ")
    if name is not None:
        print((EXAMPLES / name).read_text(encoding="utf-8"))
    return 0


def main(argv=None):
    argparse.ArgumentParser(description=__doc__).parse_args(argv)
    actions = (generate_single, generate_all, validate_names, validate_product, show_examples)
    result = 0
    while True:
        print("\n1. 单个外设草稿  2. 所有模板草稿  3. 命名 lint  4. 产品 fragment 严格验证  5. 查看模板  0. 退出")
        try:
            choice = input("请选择: ").strip()
            if choice == "0":
                return result
            if not choice.isdecimal() or not 1 <= int(choice) <= len(actions):
                print("请输入 0–5 范围内的整数。")
                continue
            status = actions[int(choice) - 1]()
            if status:
                result = 1
                print(f"操作失败，退出码 {status}")
        except (ValueError, OSError) as error:
            result = 1
            print(f"操作失败: {error}", file=sys.stderr)
        except EOFError:
            return result
        except KeyboardInterrupt:
            return 130


if __name__ == "__main__":
    sys.exit(main())
