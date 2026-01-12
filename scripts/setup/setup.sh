#!/bin/bash
# Nexus 环境搭建脚本 - Linux/macOS 版本
# 使用方法: ./setup.sh [选项]

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# 默认参数
PLATFORM="native"
INSTALL_DEV=false
INSTALL_DOCS=false
RUN_TEST=false
VERBOSE=false

# 打印函数
print_header() {
    echo -e "\n${BOLD}${CYAN}============================================================${NC}"
    echo -e "${BOLD}${CYAN}$(printf '%*s' $(((60-${#1})/2)) '')$1$(printf '%*s' $(((60-${#1})/2)) '')${NC}"
    echo -e "${BOLD}${CYAN}============================================================${NC}"
}

print_step() {
    echo -e "\n${BOLD}${BLUE}[步骤] $1${NC}"
    [[ "$VERBOSE" == "true" ]] && echo -e "${BLUE}[VERBOSE] Starting step: $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
    [[ "$VERBOSE" == "true" ]] && echo -e "${GREEN}[VERBOSE] Success: $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
    [[ "$VERBOSE" == "true" ]] && echo -e "${YELLOW}[VERBOSE] Warning: $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
    [[ "$VERBOSE" == "true" ]] && echo -e "${RED}[VERBOSE] Error: $1${NC}"
}

print_verbose() {
    [[ "$VERBOSE" == "true" ]] && echo -e "${BLUE}[VERBOSE] $1${NC}"
}

# 显示帮助
show_help() {
    echo "Nexus 环境搭建脚本 - Enhanced Version"
    echo ""
    echo "使用方法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -p, --platform PLATFORM  目标平台: native, stm32f4, all (默认: native)"
    echo "  -d, --dev                 安装开发工具 (格式化、静态分析等)"
    echo "      --docs                安装文档生成工具"
    echo "      --test                运行环境验证测试"
    echo "  -v, --verbose             详细输出"
    echo "  -h, --help                显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  $0                        # 安装基础环境"
    echo "  $0 -p stm32f4 -d         # 安装 STM32F4 开发环境和开发工具"
    echo "  $0 --docs --test -v       # 安装文档工具并运行测试，详细输出"
    echo ""
    echo "包管理器支持:"
    echo "  Linux:   apt, yum, dnf, pacman (自动检测)"
    echo "  macOS:   homebrew (自动安装如果需要)"
    echo ""
    echo "更多信息: https://github.com/nexus-platform/nexus"
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--platform)
            PLATFORM="$2"
            shift 2
            ;;
        -d|--dev)
            INSTALL_DEV=true
            shift
            ;;
        --docs)
            INSTALL_DOCS=true
            shift
            ;;
        --test)
            RUN_TEST=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
done

# 检查命令是否存在
check_command() {
    if command -v "$1" >/dev/null 2>&1; then
        print_verbose "Command '$1' is available"
        return 0
    else
        print_verbose "Command '$1' is not available"
        return 1
    fi
}

# 运行命令函数
run_command() {
    local cmd="$1"
    local timeout="${2:-30}"
    local show_output="${3:-true}"

    if [[ "$show_output" == "true" ]]; then
        echo "运行: $cmd"
    fi
    print_verbose "Running command: $cmd"

    local start_time=$(date +%s)
    if timeout "$timeout" bash -c "$cmd"; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        print_verbose "Command completed successfully in ${duration}s"
        return 0
    else
        local exit_code=$?
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        if [[ $exit_code -eq 124 ]]; then
            print_verbose "Command timed out after ${timeout}s"
        else
            print_verbose "Command failed after ${duration}s with exit code $exit_code"
        fi
        return $exit_code
    fi
}

# 获取系统信息
get_system_info() {
    print_step "检测系统信息"

    OS=$(uname -s)
    ARCH=$(uname -m)

    echo "操作系统: $OS"
    echo "架构: $ARCH"

    if [[ "$OS" == "Linux" ]]; then
        if [[ -f /etc/os-release ]]; then
            . /etc/os-release
            echo "发行版: $NAME $VERSION"
        fi
    elif [[ "$OS" == "Darwin" ]]; then
        echo "版本: $(sw_vers -productVersion)"
    fi
}

# 检测包管理器
detect_package_manager() {
    if check_command apt-get; then
        PKG_MANAGER="apt"
    elif check_command yum; then
        PKG_MANAGER="yum"
    elif check_command dnf; then
        PKG_MANAGER="dnf"
    elif check_command pacman; then
        PKG_MANAGER="pacman"
    elif check_command brew; then
        PKG_MANAGER="brew"
    else
        print_error "未找到支持的包管理器"
        exit 1
    fi

    echo "包管理器: $PKG_MANAGER"
}

# 安装基础工具
install_basic_tools() {
    print_step "安装基础工具"

    local install_cmd=""
    local success=true

    case $PKG_MANAGER in
        apt)
            print_verbose "Updating package lists..."
            if run_command "sudo apt-get update" 60; then
                install_cmd="sudo apt-get install -y cmake gcc g++ git build-essential"
            else
                print_warning "Package list update failed, continuing anyway..."
                install_cmd="sudo apt-get install -y cmake gcc g++ git build-essential"
            fi
            ;;
        yum)
            install_cmd="sudo yum install -y cmake gcc gcc-c++ git make"
            ;;
        dnf)
            install_cmd="sudo dnf install -y cmake gcc gcc-c++ git make"
            ;;
        pacman)
            install_cmd="sudo pacman -S --noconfirm cmake gcc git make"
            ;;
        brew)
            # Check if Homebrew is installed, install if needed
            if ! check_command brew; then
                print_step "安装 Homebrew..."
                if run_command '/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"' 300; then
                    print_success "Homebrew 安装成功"
                else
                    print_error "Homebrew 安装失败"
                    return 1
                fi
            fi
            install_cmd="brew install cmake git"
            ;;
    esac

    print_verbose "Installing basic tools with command: $install_cmd"
    if run_command "$install_cmd" 300; then
        print_success "基础工具安装完成"

        # Verify installation
        local tools=("cmake" "git")
        if [[ "$PKG_MANAGER" != "brew" ]]; then
            tools+=("gcc")
        fi

        for tool in "${tools[@]}"; do
            if check_command "$tool"; then
                print_verbose "$tool installed successfully"
            else
                print_warning "$tool installation may have failed"
                success=false
            fi
        done
    else
        print_error "基础工具安装失败"
        success=false
    fi

    return $([ "$success" = true ] && echo 0 || echo 1)
}

# 安装 ARM 工具链
install_arm_toolchain() {
    print_step "安装 ARM GCC 工具链"

    local install_cmd=""
    local success=false

    case $PKG_MANAGER in
        apt)
            install_cmd="sudo apt-get install -y gcc-arm-none-eabi"
            ;;
        yum|dnf)
            install_cmd="sudo $PKG_MANAGER install -y arm-none-eabi-gcc-cs"
            ;;
        pacman)
            install_cmd="sudo pacman -S --noconfirm arm-none-eabi-gcc"
            ;;
        brew)
            install_cmd="brew install --cask gcc-arm-embedded"
            ;;
    esac

    print_verbose "Installing ARM toolchain with command: $install_cmd"
    if run_command "$install_cmd" 300; then
        success=true
    else
        print_warning "Package manager installation failed, trying manual installation..."

        # Try manual installation
        if install_arm_toolchain_manual; then
            success=true
        fi
    fi

    # Verify installation
    if check_command arm-none-eabi-gcc; then
        print_success "ARM 工具链安装成功"
        local version=$(arm-none-eabi-gcc --version | head -1)
        print_verbose "ARM GCC version: $version"
        return 0
    else
        if [[ "$success" == "true" ]]; then
            print_warning "ARM 工具链安装完成，但可能需要重启终端或更新 PATH"
        else
            print_warning "ARM 工具链安装失败，请手动安装"
        fi
        return 1
    fi
}

# 手动安装 ARM 工具链
install_arm_toolchain_manual() {
    print_step "手动安装 ARM GCC 工具链"

    local tools_dir="$HOME/nexus-tools"
    local arm_version="10.3-2021.10"
    local os_suffix=""

    # Determine OS suffix
    case "$(uname -s)" in
        Linux)
            case "$(uname -m)" in
                x86_64) os_suffix="linux-x86_64" ;;
                aarch64) os_suffix="linux-aarch64" ;;
                *)
                    print_error "Unsupported Linux architecture: $(uname -m)"
                    return 1
                    ;;
            esac
            ;;
        Darwin)
            os_suffix="mac"
            ;;
        *)
            print_error "Unsupported OS for manual installation: $(uname -s)"
            return 1
            ;;
    esac

    local arm_url="https://developer.arm.com/-/media/Files/downloads/gnu-rm/${arm_version}/gcc-arm-none-eabi-${arm_version}-${os_suffix}.tar.bz2"
    local archive_name="gcc-arm-none-eabi-${arm_version}-${os_suffix}.tar.bz2"
    local archive_path="$tools_dir/$archive_name"

    # Create tools directory
    mkdir -p "$tools_dir"
    print_verbose "Created tools directory: $tools_dir"

    # Download
    print_verbose "Downloading from: $arm_url"
    if run_command "curl -L -o '$archive_path' '$arm_url'" 300; then
        print_success "下载完成"
    else
        print_error "下载失败"
        return 1
    fi

    # Extract
    print_verbose "Extracting to: $tools_dir"
    if run_command "cd '$tools_dir' && tar -xjf '$archive_name'" 120; then
        print_success "解压完成"

        # Add to PATH for current session
        local gcc_bin_dir="$tools_dir/gcc-arm-none-eabi-${arm_version}/bin"
        if [[ -d "$gcc_bin_dir" ]]; then
            export PATH="$gcc_bin_dir:$PATH"
            print_verbose "Added to current session PATH: $gcc_bin_dir"

            # Suggest permanent PATH addition
            echo ""
            print_warning "请将以下路径添加到您的 shell 配置文件 (~/.bashrc, ~/.zshrc 等):"
            echo "export PATH=\"$gcc_bin_dir:\$PATH\""
            echo ""
        fi

        # Cleanup
        rm -f "$archive_path"
        print_verbose "Cleaned up archive file"

        return 0
    else
        print_error "解压失败"
        return 1
    fi
}

# 安装开发工具
install_dev_tools() {
    print_step "安装开发工具"

    local install_cmd=""
    local success=true

    case $PKG_MANAGER in
        apt)
            install_cmd="sudo apt-get install -y clang-format clang-tidy"
            ;;
        yum|dnf)
            install_cmd="sudo $PKG_MANAGER install -y clang-tools-extra"
            ;;
        pacman)
            install_cmd="sudo pacman -S --noconfirm clang"
            ;;
        brew)
            install_cmd="brew install clang-format"
            ;;
    esac

    print_verbose "Installing dev tools with command: $install_cmd"
    if run_command "$install_cmd" 300; then
        print_success "开发工具安装完成"

        # Verify installation
        local tools=("clang-format")
        for tool in "${tools[@]}"; do
            if check_command "$tool"; then
                local version=$($tool --version | head -1)
                print_verbose "$tool installed: $version"
            else
                print_warning "$tool installation may have failed"
                success=false
            fi
        done
    else
        print_error "开发工具安装失败"
        success=false
    fi

    return $([ "$success" = true ] && echo 0 || echo 1)
}

# 安装文档工具
install_docs_tools() {
    print_step "安装文档生成工具"

    local install_cmd=""
    local success=true

    # 安装 Doxygen
    case $PKG_MANAGER in
        apt)
            install_cmd="sudo apt-get install -y doxygen python3-pip"
            ;;
        yum|dnf)
            install_cmd="sudo $PKG_MANAGER install -y doxygen python3-pip"
            ;;
        pacman)
            install_cmd="sudo pacman -S --noconfirm doxygen python-pip"
            ;;
        brew)
            install_cmd="brew install doxygen"
            ;;
    esac

    print_verbose "Installing docs tools with command: $install_cmd"
    if run_command "$install_cmd" 300; then
        print_success "Doxygen 安装完成"
    else
        print_warning "Doxygen 安装失败"
        success=false
    fi

    # 安装 Python 文档包
    print_step "安装 Python 文档包"
    local pip_cmd=""
    if check_command pip3; then
        pip_cmd="pip3 install --user sphinx breathe sphinx-rtd-theme"
    elif check_command pip; then
        pip_cmd="pip install --user sphinx breathe sphinx-rtd-theme"
    else
        print_warning "pip 不可用，跳过 Python 文档包安装"
        return $([ "$success" = true ] && echo 0 || echo 1)
    fi

    print_verbose "Installing Python docs packages with command: $pip_cmd"
    if run_command "$pip_cmd" 180; then
        print_success "Python 文档包安装完成"

        # Verify installation
        if python3 -c "import sphinx, breathe" 2>/dev/null; then
            print_verbose "Python documentation packages verified successfully"
        else
            print_warning "Python documentation packages may not be properly installed"
            success=false
        fi
    else
        print_warning "Python 文档包安装失败"
        success=false
    fi

    return $([ "$success" = true ] && echo 0 || echo 1)
}

# 验证安装
verify_installation() {
    print_step "验证安装"

    local tools=("cmake" "git")
    local success_count=0
    local total_count=${#tools[@]}

    # 添加编译器
    if [[ "$PKG_MANAGER" != "brew" ]]; then
        tools+=("gcc" "g++")
        total_count=$((total_count + 2))
    else
        # macOS 使用 clang
        tools+=("clang" "clang++")
        total_count=$((total_count + 2))
    fi

    # 添加 ARM 工具链 (如果需要)
    if [[ "$PLATFORM" == "stm32f4" || "$PLATFORM" == "all" ]]; then
        tools+=("arm-none-eabi-gcc")
        total_count=$((total_count + 1))
    fi

    # 添加开发工具 (如果安装了)
    if [[ "$INSTALL_DEV" == true ]]; then
        tools+=("clang-format")
        total_count=$((total_count + 1))
    fi

    # 添加文档工具 (如果安装了)
    if [[ "$INSTALL_DOCS" == true ]]; then
        tools+=("doxygen")
        total_count=$((total_count + 1))
    fi

    # 检查工具
    echo "检查已安装的工具:"
    for tool in "${tools[@]}"; do
        if check_command "$tool"; then
            local version=""
            case "$tool" in
                cmake)
                    version=$(cmake --version | head -1 | cut -d' ' -f3)
                    ;;
                git)
                    version=$(git --version | cut -d' ' -f3)
                    ;;
                gcc|clang)
                    version=$($tool --version | head -1 | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' | head -1)
                    ;;
                arm-none-eabi-gcc)
                    version=$($tool --version | head -1 | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' | head -1)
                    ;;
                doxygen)
                    version=$($tool --version 2>/dev/null | head -1)
                    ;;
                *)
                    version=$($tool --version 2>/dev/null | head -1 | grep -o '[0-9]\+\.[0-9]\+' | head -1)
                    ;;
            esac

            if [[ -n "$version" ]]; then
                print_success "$tool 可用 (版本: $version)"
            else
                print_success "$tool 可用"
            fi
            success_count=$((success_count + 1))
        else
            print_error "$tool 不可用"
        fi
    done

    echo ""
    local success_rate=$((success_count * 100 / total_count))
    echo "验证结果: $success_count/$total_count 工具可用 (${success_rate}%)"

    print_verbose "Verification completed with $success_rate% success rate"

    if [[ $success_count -eq $total_count ]]; then
        print_success "所有工具验证通过"
        return 0
    elif [[ $success_rate -ge 80 ]]; then
        print_warning "大部分工具验证通过，环境基本可用"
        return 0
    else
        print_error "验证失败，请检查安装"
        return 1
    fi
}

# 创建 VS Code 配置
create_vscode_config() {
    print_step "创建 VS Code 配置"

    mkdir -p .vscode

    cat > .vscode/settings.json << 'EOF'
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.buildDirectory": "${workspaceFolder}/build-${buildType}",
    "files.associations": {
        "*.h": "c",
        "*.c": "c"
    },
    "editor.formatOnSave": true,
    "C_Cpp.clang_format_style": "file"
}
EOF

    print_success "VS Code 配置创建完成"
}

# 运行测试构建
run_test_build() {
    print_step "运行测试构建"

    local build_dir="build-test"
    local success=true

    # 清理旧的构建目录
    if [[ -d "$build_dir" ]]; then
        print_verbose "Cleaning up old build directory: $build_dir"
        rm -rf "$build_dir"
    fi

    # 创建构建目录
    if ! mkdir "$build_dir"; then
        print_error "无法创建构建目录: $build_dir"
        return 1
    fi

    print_verbose "Created build directory: $build_dir"
    cd "$build_dir"

    # 配置 CMake
    print_verbose "Configuring CMake..."
    local cmake_cmd="cmake -DCMAKE_BUILD_TYPE=Debug -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON .."
    if run_command "$cmake_cmd" 120; then
        print_success "CMake 配置成功"
    else
        print_error "CMake 配置失败"
        cd ..
        return 1
    fi

    # 构建项目
    print_verbose "Building project..."
    local build_cmd="cmake --build . --config Debug"
    if run_command "$build_cmd" 300; then
        print_success "测试构建成功"

        # 检查构建产物
        if [[ -f "applications/blinky/blinky" ]] || [[ -f "applications/blinky/blinky.exe" ]]; then
            print_verbose "Build artifacts found successfully"
        else
            print_warning "构建完成但未找到预期的构建产物"
            success=false
        fi
    else
        print_error "构建失败"
        success=false
    fi

    cd ..

    # 清理测试构建目录 (可选)
    if [[ "$success" == true ]]; then
        print_verbose "Cleaning up test build directory"
        rm -rf "$build_dir"
    else
        print_verbose "Keeping test build directory for debugging: $build_dir"
    fi

    return $([ "$success" = true ] && echo 0 || echo 1)
}

# 主函数
main() {
    local start_time=$(date +%s)

    print_header "Nexus 环境搭建脚本 - Enhanced Version"

    echo "配置信息:"
    echo "  目标平台: $PLATFORM"
    echo "  安装开发工具: $([ "$INSTALL_DEV" = true ] && echo "是" || echo "否")"
    echo "  安装文档工具: $([ "$INSTALL_DOCS" = true ] && echo "是" || echo "否")"
    echo "  运行测试: $([ "$RUN_TEST" = true ] && echo "是" || echo "否")"
    echo "  详细输出: $([ "$VERBOSE" = true ] && echo "是" || echo "否")"

    # 获取系统信息
    get_system_info

    # 检测包管理器
    print_step "检测包管理器"
    detect_package_manager
    print_verbose "Using package manager: $PKG_MANAGER"

    # 安装步骤计数
    local total_steps=3  # 基础步骤: 基础工具、验证、VS Code配置
    local current_step=0

    # 计算总步骤数
    if [[ "$PLATFORM" == "stm32f4" || "$PLATFORM" == "all" ]]; then
        total_steps=$((total_steps + 1))
    fi
    if [[ "$INSTALL_DEV" == true ]]; then
        total_steps=$((total_steps + 1))
    fi
    if [[ "$INSTALL_DOCS" == true ]]; then
        total_steps=$((total_steps + 1))
    fi
    if [[ "$RUN_TEST" == true ]]; then
        total_steps=$((total_steps + 1))
    fi

    # 安装基础工具
    current_step=$((current_step + 1))
    echo ""
    echo "进度: [$current_step/$total_steps] 安装基础工具"
    if ! install_basic_tools; then
        print_error "基础工具安装失败，终止安装"
        return 1
    fi

    # 安装 ARM 工具链
    if [[ "$PLATFORM" == "stm32f4" || "$PLATFORM" == "all" ]]; then
        current_step=$((current_step + 1))
        echo ""
        echo "进度: [$current_step/$total_steps] 安装 ARM 工具链"
        if ! install_arm_toolchain; then
            print_warning "ARM 工具链安装失败，但继续其他安装"
        fi
    fi

    # 安装开发工具
    if [[ "$INSTALL_DEV" == true ]]; then
        current_step=$((current_step + 1))
        echo ""
        echo "进度: [$current_step/$total_steps] 安装开发工具"
        if ! install_dev_tools; then
            print_warning "开发工具安装失败，但继续其他安装"
        fi
    fi

    # 安装文档工具
    if [[ "$INSTALL_DOCS" == true ]]; then
        current_step=$((current_step + 1))
        echo ""
        echo "进度: [$current_step/$total_steps] 安装文档工具"
        if ! install_docs_tools; then
            print_warning "文档工具安装失败，但继续其他安装"
        fi
    fi

    # 验证安装
    current_step=$((current_step + 1))
    echo ""
    echo "进度: [$current_step/$total_steps] 验证安装"
    local verification_result=0
    if ! verify_installation; then
        print_warning "部分工具验证失败，请检查安装"
        verification_result=1
    fi

    # 创建 VS Code 配置
    current_step=$((current_step + 1))
    echo ""
    echo "进度: [$current_step/$total_steps] 创建 VS Code 配置"
    create_vscode_config

    # 运行测试
    if [[ "$RUN_TEST" == true ]]; then
        current_step=$((current_step + 1))
        echo ""
        echo "进度: [$current_step/$total_steps] 运行测试构建"
        if ! run_test_build; then
            print_error "测试构建失败"
            return 1
        fi
    fi

    # 计算总耗时
    local end_time=$(date +%s)
    local total_duration=$((end_time - start_time))
    local minutes=$((total_duration / 60))
    local seconds=$((total_duration % 60))

    print_header "环境搭建完成"
    print_success "Nexus 开发环境已准备就绪!"
    echo ""
    echo "安装统计:"
    echo "  总耗时: ${minutes}分${seconds}秒"
    echo "  完成步骤: $current_step/$total_steps"

    # 显示下一步操作
    echo ""
    echo "下一步操作:"
    echo "1. 重启终端以确保环境变量生效"
    echo "2. 运行构建脚本: python scripts/building/build.py"
    echo "3. 运行测试: python scripts/test/test.py"

    if [[ "$PLATFORM" == "stm32f4" || "$PLATFORM" == "all" ]]; then
        echo ""
        echo "STM32F4 开发:"
        echo "  构建固件: python scripts/building/build.py -p stm32f4"
        echo "  输出位置: build-stm32f4/applications/blinky/"
    fi

    echo ""
    echo "有用的命令:"
    echo "  检查环境: python scripts/setup/check-env.py"
    echo "  格式化代码: python scripts/tools/format.py"
    echo "  生成文档: python scripts/tools/docs.py"

    return $verification_result
}

# 运行主函数
if main "$@"; then
    print_verbose "Script completed successfully"
    exit 0
else
    print_verbose "Script completed with errors"
    exit 1
fi
