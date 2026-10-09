# 可重定位 Source SDK

本包分发源码，消费工程重新生成有效配置并编译实际平台、startup 和 linker；它不是 binary SDK，也没有跨编译器/配置的预编译 ABI 承诺。当前维护范围是 Native、STM32F407 与 GD32F470，包准备不下载依赖。

## 准备与校验

必须使用干净、真实 Git checkout，并初始化 `ext/freertos`、`vendors/arm/CMSIS_5`、STM32F4 device/HAL 及其必要递归依赖。GD32 reviewed import、许可证与固定工具链 lock 同时绑定。

```sh
python3 -B cmake/package/package_source_sdk.py \
  --source /path/to/nexus --output '/path/to/nexus sdk prefix'
python3 -B '/path/to/nexus sdk prefix/share/nexus/src/cmake/package/package_source_sdk.py' \
  --verify '/path/to/nexus sdk prefix/share/nexus/src'
```

已配置 Nexus 的完整 checkout 也可调用：

```cmake
nexus_package_source_sdk(OUTPUT_DIRECTORY "/path/to/nexus sdk prefix")
```

输出必须在输入 checkout 外且尚不存在；失败不会留下半个包。移动整个 prefix 即可重定位：`lib/cmake/Nexus` 保存 package config/version，`share/nexus/src` 保存真实源码、依赖、许可和 `.nexus-source-sdk.json`。固定 Git 依赖使用原始 committed blobs 导出，避免 checkout 的 EOL filter 改变 commit 字节身份。

Manifest 记录真实 source commit/tree、完整逐文件摘要、所有必要依赖 commit/tree、GD32 source import 与工具链 lock；两个导出 CMake 文件另外绑定摘要。哈希用于内容与 provenance 一致性，包是未签名源码快照，`publishable=true` 不代表产品发布 promotion 或实板资格。

## 外部消费

父工程须选择自己的配置、Board/layout 和工具链；一组 build root 只允许一份 Nexus。普通应用通过公开目标和头文件消费，两个版本的包不能在同一次构建内混用。

```cmake
cmake_minimum_required(VERSION 3.21)
project(external_firmware LANGUAGES C CXX ASM)
find_package(Nexus 0.1.0 EXACT CONFIG REQUIRED)
nexus_add_application(TARGET firmware SOURCES main.c)
```

```sh
cmake -S /path/to/application -B /path/to/application-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  '-DNexus_DIR=/path/to/nexus sdk prefix/lib/cmake/Nexus' \
  '-DNEXUS_CONFIG_FILE=/path/to/application/platform.conf' \
  -DNEXUS_EXPECTED_SOURCE_REVISION=<expected-full-source-commit>
cmake --build /path/to/application-build
```

ARM 工程在首次 configure、`project()` 之前通过 `CMAKE_TOOLCHAIN_FILE=<prefix>/share/nexus/src/cmake/toolchains/arm-gcc.cmake` 选择工具链，并设定 `NEXUS_PLATFORM`。工具链程序由调用者提供并按包内 lock/交付流程验证，不随 SDK 捆绑；有效配置、工具链、Board、layout 与工件身份属于该消费构建。

本包不包含平台开发测试目录与 Googletest，不接受 `NEXUS_BUILD_TESTS=ON` 或 `NEXUS_BUILD_CONTRACTS=ON`。完整 checkout 的契约/质量门禁独立执行，外部应用的业务与实板测试由其仓库拥有。

## 开发 fixture

架构重构工作树可以显式使用 `--development-fixture`，或 CMake `DEVELOPMENT_FIXTURE` 参数，生成 `publishable=false` 的独立 snapshot。它仍校验全部必要依赖和真实文件，不伪造“当前 commit 已含所有修改”。消费必须显式开启 `NEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON`；基础构建 source identity 会增加 snapshot 摘要，不得用于发布 promotion。

开发回归运行 `python3 -B cmake/package/test_source_sdk.py -v`：移动包到含空格路径，在不同父 Git 身份下构建/运行 C 与 C++ Native consumer，并在 ARM GNU 可用时真实编译两个 MCU ELF、验证真实向量/startup/linker。缺少 ARM 时明确 skip，不能据此宣称 ARM 验证。干净提交交付前应另外以默认严格模式准备 source package，并重新执行其消费矩阵。
