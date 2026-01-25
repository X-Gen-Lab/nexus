#!/usr/bin/env python3
"""
Nexus Documentation Optimization Verification Script
验证文档优化是否正确完成

Usage:
    python verify_optimization.py
"""

import os
import sys
from pathlib import Path
from typing import List, Tuple

# ANSI color codes
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def print_header(msg: str):
    print(f"\n{Colors.BLUE}{Colors.BOLD}{'='*70}{Colors.RESET}")
    print(f"{Colors.BLUE}{Colors.BOLD}{msg:^70}{Colors.RESET}")
    print(f"{Colors.BLUE}{Colors.BOLD}{'='*70}{Colors.RESET}\n")

def print_check(msg: str, passed: bool):
    status = f"{Colors.GREEN}✓{Colors.RESET}" if passed else f"{Colors.RED}✗{Colors.RESET}"
    print(f"{status} {msg}")

def print_info(msg: str):
    print(f"{Colors.YELLOW}ℹ{Colors.RESET} {msg}")

def check_file_exists(path: Path, description: str) -> bool:
    """Check if a file exists."""
    exists = path.exists()
    print_check(f"{description}: {path}", exists)
    return exists

def check_directory_exists(path: Path, description: str) -> bool:
    """Check if a directory exists."""
    exists = path.is_dir()
    print_check(f"{description}: {path}", exists)
    return exists

def check_file_content(path: Path, search_str: str, description: str) -> bool:
    """Check if a file contains specific content."""
    if not path.exists():
        print_check(f"{description}", False)
        return False

    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
        found = search_str in content
        print_check(f"{description}", found)
        return found

def verify_new_files() -> Tuple[int, int]:
    """Verify that all new files were created."""
    print_header("Checking New Files")

    checks = [
        (Path('DOCUMENTATION_GUIDE.rst'), "Documentation guide"),
        (Path('QUICK_REFERENCE.rst'), "Quick reference"),
        (Path('README.md'), "Build system README"),
        (Path('translate_helper.py'), "Translation helper tool"),
        (Path('_static/custom.css'), "Custom CSS"),
        (Path('../DOCUMENTATION_OPTIMIZATION.md'), "Optimization report"),
        (Path('../OPTIMIZATION_SUMMARY.md'), "Optimization summary"),
    ]

    passed = sum(check_file_exists(path, desc) for path, desc in checks)
    total = len(checks)

    return passed, total

def verify_modified_files() -> Tuple[int, int]:
    """Verify that key files were modified correctly."""
    print_header("Checking Modified Files")

    checks = [
        (Path('index.rst'), 'DOCUMENTATION_GUIDE', "index.rst includes DOCUMENTATION_GUIDE"),
        (Path('index.rst'), 'QUICK_REFERENCE', "index.rst includes QUICK_REFERENCE"),
        (Path('index.rst'), 'Key Features', "index.rst has Key Features section"),
        (Path('tutorials/index.rst'), 'Tutorials', "tutorials/index.rst is in English"),
        (Path('tutorials/index.rst'), 'Learning Path', "tutorials/index.rst has Learning Path"),
        (Path('conf.py'), 'custom.css', "conf.py includes custom.css"),
        (Path('Makefile'), 'stats', "Makefile has stats target"),
        (Path('Makefile'), 'validate', "Makefile has validate target"),
    ]

    passed = sum(check_file_content(path, search, desc) for path, search, desc in checks)
    total = len(checks)

    return passed, total

def verify_translation_system() -> Tuple[int, int]:
    """Verify translation system setup."""
    print_header("Checking Translation System")

    checks = [
        (Path('locale'), "locale directory"),
        (Path('locale/zh_CN'), "Chinese locale directory"),
        (Path('locale/zh_CN/LC_MESSAGES'), "Chinese LC_MESSAGES directory"),
        (Path('translate_helper.py'), "Translation helper script"),
    ]

    passed = sum(check_directory_exists(path, desc) if 'directory' in desc
                 else check_file_exists(path, desc) for path, desc in checks)
    total = len(checks)

    # Check if translate_helper.py is executable
    helper_path = Path('translate_helper.py')
    if helper_path.exists():
        with open(helper_path, 'r', encoding='utf-8') as f:
            content = f.read()
            has_shebang = content.startswith('#!/usr/bin/env python3')
            print_check("translate_helper.py has shebang", has_shebang)
            if has_shebang:
                passed += 1
            total += 1

    return passed, total

def verify_build_system() -> Tuple[int, int]:
    """Verify build system enhancements."""
    print_header("Checking Build System")

    checks = [
        (Path('build_docs.py'), "Build script exists"),
        (Path('Makefile'), "Makefile exists"),
        (Path('conf.py'), "Sphinx config exists"),
        (Path('requirements.txt'), "Requirements file exists"),
    ]

    passed = sum(check_file_exists(path, desc) for path, desc in checks)
    total = len(checks)

    # Check Makefile targets
    makefile_path = Path('Makefile')
    if makefile_path.exists():
        with open(makefile_path, 'r', encoding='utf-8') as f:
            content = f.read()
            targets = ['html-all', 'stats', 'validate', 'auto-trans', 'doxygen', 'linkcheck', 'full']
            for target in targets:
                has_target = f'.PHONY: {target}' in content or f'{target}:' in content
                print_check(f"Makefile has '{target}' target", has_target)
                if has_target:
                    passed += 1
                total += 1

    return passed, total

def verify_documentation_structure() -> Tuple[int, int]:
    """Verify documentation structure."""
    print_header("Checking Documentation Structure")

    directories = [
        'getting_started',
        'user_guide',
        'tutorials',
        'platform_guides',
        'api',
        'reference',
        'development',
        '_static',
        '_templates',
        'locale',
    ]

    passed = sum(check_directory_exists(Path(d), f"{d} directory") for d in directories)
    total = len(directories)

    return passed, total

def verify_styling() -> Tuple[int, int]:
    """Verify custom styling."""
    print_header("Checking Custom Styling")

    css_path = Path('_static/custom.css')
    if not css_path.exists():
        print_check("custom.css exists", False)
        return 0, 1

    with open(css_path, 'r', encoding='utf-8') as f:
        content = f.read()

    checks = [
        ('body {', "Body styles defined"),
        ('.highlight', "Code block styles defined"),
        ('.admonition', "Admonition styles defined"),
        ('table.docutils', "Table styles defined"),
        ('@media (max-width: 768px)', "Responsive styles defined"),
        ('@media print', "Print styles defined"),
    ]

    passed = sum(1 for search, desc in checks if (print_check(desc, search in content), search in content)[1])
    total = len(checks)

    return passed, total

def verify_python_scripts() -> Tuple[int, int]:
    """Verify Python scripts are valid."""
    print_header("Checking Python Scripts")

    scripts = [
        'build_docs.py',
        'translate_helper.py',
        'verify_optimization.py',
    ]

    passed = 0
    total = len(scripts)

    for script in scripts:
        script_path = Path(script)
        if not script_path.exists():
            print_check(f"{script} exists", False)
            continue

        try:
            with open(script_path, 'r', encoding='utf-8') as f:
                content = f.read()
                # Basic syntax check
                compile(content, script, 'exec')
                print_check(f"{script} is valid Python", True)
                passed += 1
        except SyntaxError as e:
            print_check(f"{script} is valid Python", False)
            print_info(f"  Error: {e}")

    return passed, total

def main():
    """Run all verification checks."""
    print(f"\n{Colors.BOLD}Nexus Documentation Optimization Verification{Colors.RESET}")
    print(f"{Colors.BOLD}{'='*70}{Colors.RESET}")

    # Change to script directory
    script_dir = Path(__file__).parent
    os.chdir(script_dir)

    # Run all checks
    results = []
    results.append(verify_new_files())
    results.append(verify_modified_files())
    results.append(verify_translation_system())
    results.append(verify_build_system())
    results.append(verify_documentation_structure())
    results.append(verify_styling())
    results.append(verify_python_scripts())

    # Calculate totals
    total_passed = sum(r[0] for r in results)
    total_checks = sum(r[1] for r in results)
    percentage = (total_passed / total_checks * 100) if total_checks > 0 else 0

    # Print summary
    print_header("Verification Summary")

    print(f"Total checks: {total_checks}")
    print(f"Passed: {Colors.GREEN}{total_passed}{Colors.RESET}")
    print(f"Failed: {Colors.RED}{total_checks - total_passed}{Colors.RESET}")
    print(f"Success rate: {Colors.GREEN if percentage >= 90 else Colors.YELLOW}{percentage:.1f}%{Colors.RESET}")

    if percentage >= 90:
        print(f"\n{Colors.GREEN}{Colors.BOLD}✓ Optimization verification PASSED!{Colors.RESET}")
        print(f"{Colors.GREEN}All critical components are in place.{Colors.RESET}")
        return 0
    elif percentage >= 70:
        print(f"\n{Colors.YELLOW}{Colors.BOLD}⚠ Optimization verification PARTIAL{Colors.RESET}")
        print(f"{Colors.YELLOW}Some components are missing or incomplete.{Colors.RESET}")
        return 1
    else:
        print(f"\n{Colors.RED}{Colors.BOLD}✗ Optimization verification FAILED{Colors.RESET}")
        print(f"{Colors.RED}Many components are missing or incomplete.{Colors.RESET}")
        return 2

if __name__ == '__main__':
    sys.exit(main())
