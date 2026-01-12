# Nexus 开发环境 Docker 镜像
# 基于 Ubuntu 20.04，包含所有必要的开发工具

FROM ubuntu:20.04

# 设置非交互模式
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# 设置工作目录
WORKDIR /workspace

# 安装基础工具和依赖
RUN apt-get update && apt-get install -y \
    # 基础工具
    curl \
    wget \
    git \
    vim \
    nano \
    unzip \
    # 构建工具
    cmake \
    make \
    ninja-build \
    # 编译器
    gcc \
    g++ \
    # ARM 工具链
    gcc-arm-none-eabi \
    # 开发工具
    clang-format \
    clang-tidy \
    # 文档工具
    doxygen \
    graphviz \
    # Python 和包管理
    python3 \
    python3-pip \
    # 调试工具
    gdb \
    gdb-multiarch \
    # 其他实用工具
    tree \
    htop \
    && rm -rf /var/lib/apt/lists/*

# 安装 Python 文档包
RUN pip3 install \
    sphinx \
    breathe \
    sphinx-rtd-theme \
    sphinx-tabs

# 创建开发用户
RUN useradd -m -s /bin/bash developer && \
    usermod -aG sudo developer && \
    echo "developer ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# 设置 Git 配置
USER developer
RUN git config --global user.name "Nexus Developer" && \
    git config --global user.email "developer@nexus.local" && \
    git config --global init.defaultBranch main

# 创建工作目录
RUN mkdir -p /home/developer/workspace

# 设置环境变量
ENV PATH="/usr/bin:$PATH"
ENV CC=gcc
ENV CXX=g++

# 切换到工作目录
WORKDIR /home/developer/workspace

# 暴露端口 (用于文档服务器等)
EXPOSE 8000 8080

# 默认命令
CMD ["/bin/bash"]

# 构建信息
LABEL maintainer="Nexus Team"
LABEL description="Nexus Embedded Development Environment"
LABEL version="1.0.0"