/**
 * \file            config_test.cpp
 * \brief           Host C11 configuration and static instance contracts
 * \author          Nexus Team
 * \version         0.1.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nxconfig/nxconfig.h"
extern "C" {
#include "../src/internal.h"
}
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
namespace fs = std::filesystem;
using testing::HasSubstr;

std::string read(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
}

void write(const fs::path& path, const std::string& value) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << value;
}

class HostConfiguration : public testing::Test {
  protected:
    fs::path root = NXCONFIG_REPO_ROOT;
    fs::path temporary;
    nxconfig_assembly_t* assembly = nullptr;
    nxconfig_diagnostic_t diagnostic{};

    void SetUp() override {
        std::string pattern =
            (fs::temp_directory_path() / "nxconfig-contract-XXXXXX").string();
        std::vector<char> bytes(pattern.begin(), pattern.end());
        bytes.push_back('\0');
        char* directory = mkdtemp(bytes.data());
        ASSERT_NE(directory, nullptr);
        temporary = directory;
    }

    void TearDown() override {
        nxconfig_destroy(assembly);
        if (!temporary.empty())
            fs::remove_all(temporary);
    }

    nxconfig_result_t resolve(const fs::path& source) {
        nxconfig_destroy(assembly);
        assembly = nullptr;
        diagnostic = {};
        return nxconfig_resolve(root.c_str(), source.c_str(), &assembly,
                                &diagnostic);
    }

    fs::path authored(const std::string& content) {
        fs::path path = temporary / "assembly.toml";
        write(path, content);
        return path;
    }

    std::string base() const {
        return "schema = 2\nboard = \"stm32f407_qiming_v31\"\n"
               "backend = \"baremetal\"\nclock = \"hse8-pll168\"\n";
    }
};

TEST_F(HostConfiguration, ResolvesActualGpioUartIntoNamedReadonlyRecords) {
    ASSERT_EQ(
        resolve(root / "tools/configure/assemblies/qiming-baremetal.toml"),
        NXCONFIG_OK)
        << diagnostic.context << ": " << diagnostic.message;
    const auto* platform = nxconfig_platform(assembly);
    ASSERT_NE(platform, nullptr);
    EXPECT_STREQ(platform->part, "STM32F407ZGT6");
    EXPECT_STREQ(platform->device_define, "STM32F407xx");
    EXPECT_STREQ(platform->startup, "startup_stm32f407xx.s");
    EXPECT_EQ(platform->flash_bytes, 1024U * 1024U);
    EXPECT_EQ(platform->external_irq_count, 82U);
    ASSERT_EQ(nxconfig_controller_count(assembly), 2U);
    const auto* gpio = nxconfig_controller(assembly, 0U);
    const auto* uart = nxconfig_controller(assembly, 1U);
    ASSERT_NE(gpio, nullptr);
    ASSERT_NE(uart, nullptr);
    EXPECT_EQ(gpio->kind, NXCONFIG_GPIO);
    EXPECT_EQ(gpio->options.gpio.mask, 8U);
    EXPECT_EQ(uart->kind, NXCONFIG_UART);
    EXPECT_STREQ(uart->controller, "USART1");
    EXPECT_STREQ(uart->mode, "irq-byte-event");
    EXPECT_EQ(uart->options.uart.rx_capacity, 32U);
    EXPECT_EQ(uart->options.uart.clock_hz, 84000000U);
    EXPECT_TRUE(uart->options.uart.events);
    EXPECT_EQ(nxconfig_controller(assembly, 2U), nullptr);
}

TEST_F(HostConfiguration, ResolvesActualSpiControllerAndSeparateCsEndpoint) {
    ASSERT_EQ(resolve(root / "tests/contracts/stm32_assembly/zg-spi.toml"),
              NXCONFIG_OK)
        << diagnostic.context << ": " << diagnostic.message;
    ASSERT_EQ(nxconfig_controller_count(assembly), 1U);
    EXPECT_EQ(nxconfig_controller(assembly, 0U)->kind, NXCONFIG_SPI);
    ASSERT_EQ(nxconfig_spi_endpoint_count(assembly), 1U);
    const auto* endpoint = nxconfig_spi_endpoint(assembly, 0U);
    EXPECT_STREQ(endpoint->id, "sensor0");
    EXPECT_EQ(endpoint->controller_index, 0U);
    EXPECT_STREQ(endpoint->chip_select.name, "PE3");
    EXPECT_EQ(endpoint->max_hz, 1000000U);
}

TEST_F(HostConfiguration, UnknownFieldsAndUnimplementedModesNeverDowngrade) {
    for (const auto& suffix :
         {"mystery = 1\n",
          "[uart.console]\nbinding=\"uart1\"\nmode=\"dma-tx\"\n"
          "baud=115200\n",
          "[uart.console]\nbinding=\"uart1\"\nmode=\"irq\"\n"
          "baud=115200\nunknown=true\n"}) {
        EXPECT_NE(resolve(authored(base() + suffix)), NXCONFIG_OK);
        EXPECT_EQ(assembly, nullptr);
        EXPECT_NE(diagnostic.code, NXCONFIG_OK);
        EXPECT_NE(diagnostic.context[0], '\0');
    }
}

TEST_F(HostConfiguration, DuplicateControllerAndGeneratedIdentifierReject) {
    for (const auto& suffix :
         {"[uart.a]\nbinding=\"uart1\"\nmode=\"irq\"\nbaud=115200\n"
          "[uart.b]\nbinding=\"uart1\"\nmode=\"irq\"\nbaud=115200\n",
          "[gpio.a-b]\nbinding=\"led0\"\nmode=\"output\"\n"
          "[gpio.a_b]\nbinding=\"led0\"\nmode=\"output\"\n",
          "[gpio.count]\nbinding=\"led0\"\nmode=\"output\"\n"}) {
        EXPECT_EQ(resolve(authored(base() + suffix)), NXCONFIG_CONFLICT);
        EXPECT_EQ(assembly, nullptr);
    }
}

TEST_F(HostConfiguration, SelectedSpiChipSelectConflictsWithAnotherOwner) {
    auto text = read(root / "tests/contracts/stm32_assembly/zg-spi.toml");
    const auto board = root / "tests/contracts/stm32_assembly/zg_board";
    const auto at = text.find("board_package = \"zg_board\"");
    ASSERT_NE(at, std::string::npos);
    text.replace(at, std::string("board_package = \"zg_board\"").size(),
                 "board_package = \"" + board.string() + "\"");
    text += "\n[gpio.duplicate]\nbinding=\"cs0\"\nmode=\"output\"\n";
    EXPECT_EQ(resolve(authored(text)), NXCONFIG_CONFLICT);
    EXPECT_EQ(assembly, nullptr);
    EXPECT_THAT(diagnostic.message, HasSubstr("PE3"));
}

TEST_F(HostConfiguration, UartCapacityAndPriorityAreStrictTypedBounds) {
    for (const auto& invalid :
         {"capacity=0", "capacity=true", "capacity=65536"}) {
        auto text = base() +
                    "[uart.console]\nbinding=\"uart1\"\nmode=\"irq\"\n"
                    "baud=115200\n[uart.console.rx]\nformat=\"bytes\"\n" +
                    invalid + "\n";
        EXPECT_EQ(resolve(authored(text)), NXCONFIG_INVALID);
        EXPECT_EQ(assembly, nullptr);
    }
    auto text = base() + "[uart.console]\nbinding=\"uart1\"\nmode=\"irq\"\n"
                         "baud=115200\n[uart.console.irq]\npriority=16\n";
    EXPECT_EQ(resolve(authored(text)), NXCONFIG_INVALID);
}

TEST_F(HostConfiguration, SourceFactsDoNotRequireAnInitializedMcuSdk) {
    const auto original = root;
    root = temporary / "source";
    fs::create_directories(root / "soc/stm32");
    fs::copy(original / "soc/stm32/stm32f4", root / "soc/stm32/stm32f4",
             fs::copy_options::recursive);
    fs::create_directories(root / "boards");
    fs::copy(original / "boards/stm32f407_qiming_v31",
             root / "boards/stm32f407_qiming_v31", fs::copy_options::recursive);
    EXPECT_FALSE(fs::exists(root / "vendors"));
    EXPECT_EQ(resolve(authored(base())), NXCONFIG_OK)
        << diagnostic.context << ": " << diagnostic.message;
}

TEST_F(HostConfiguration, RejectsDuplicateJsonFactKeysWithoutReturningAnOwner) {
    const auto board = temporary / "board";
    fs::create_directories(board);
    auto json = read(root / "boards/stm32f407_qiming_v31/board.json");
    json.insert(json.find('{') + 1U, "\"soc_family\":\"stm32f4\",");
    write(board / "board.json", json);
    auto text = "schema=2\nboard_package=\"" + board.string() +
                "\"\nbackend=\"baremetal\"\nclock=\"hse8-pll168\"\n";
    EXPECT_EQ(resolve(authored(text)), NXCONFIG_INVALID);
    EXPECT_EQ(assembly, nullptr);
    EXPECT_THAT(diagnostic.message, HasSubstr("Duplicate"));
}

TEST_F(HostConfiguration, EmitsOnlySelectedStaticObjectsAndTypedLookup) {
    ASSERT_EQ(
        resolve(root / "tools/configure/assemblies/qiming-baremetal.toml"),
        NXCONFIG_OK);
    auto output = temporary / "generated";
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic), NXCONFIG_OK)
        << diagnostic.message;
    auto source = read(output / "nexus_instances.c");
    EXPECT_THAT(source, HasSubstr("nx_stm32_uart_cold_config_t"));
    EXPECT_THAT(source, HasSubstr("nx_uart_rx_event_t"));
    EXPECT_THAT(source, HasSubstr("nx_stm32_uart_ops"));
    EXPECT_THAT(source, HasSubstr("nx_factory_uart"));
    EXPECT_EQ(source.find("RCC->"), std::string::npos);
    EXPECT_EQ(source.find("HAL_"), std::string::npos);
    EXPECT_EQ(source.find("malloc("), std::string::npos);
    EXPECT_TRUE(fs::is_regular_file(output / "nexus_factory_strong.h"));
    EXPECT_TRUE(fs::is_regular_file(output / "manifest.json"));
}

TEST_F(HostConfiguration, EqualGenerationPreservesBytesAndTimestamps) {
    ASSERT_EQ(resolve(authored(base() + "[gpio.indicator]\nbinding=\"led0\"\n"
                                        "mode=\"output\"\n")),
              NXCONFIG_OK);
    const auto output = temporary / "generated";
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_OK);
    const auto before = read(output / "nexus_instances.c");
    const auto time = fs::last_write_time(output / "nexus_instances.c");
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_OK);
    EXPECT_EQ(read(output / "nexus_instances.c"), before);
    EXPECT_EQ(fs::last_write_time(output / "nexus_instances.c"), time);
}

TEST_F(HostConfiguration, UnownedOutputAndChangedInputsAreRejected) {
    const auto input = authored(base());
    ASSERT_EQ(resolve(input), NXCONFIG_OK);
    const auto output = temporary / "generated";
    write(output / "valuable.txt", "original");
    EXPECT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_IO);
    EXPECT_EQ(read(output / "valuable.txt"), "original");
    fs::remove_all(output);
    write(input, base() + "# changed after resolution\n");
    EXPECT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_IO);
    EXPECT_FALSE(fs::exists(output));
}

TEST_F(HostConfiguration, GeneratedFilesRemainPrivateWithAnOpenProcessMask) {
    ASSERT_EQ(resolve(authored(base())), NXCONFIG_OK);
    struct Mask {
        mode_t previous = umask(0);
        ~Mask() {
            umask(previous);
        }
    } mask;
    const auto output = temporary / "generated";
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_OK);
    for (const auto& entry : fs::directory_iterator(output)) {
        EXPECT_EQ(fs::status(entry).permissions() &
                      (fs::perms::group_all | fs::perms::others_all),
                  fs::perms::none)
            << entry.path();
    }
}

TEST_F(HostConfiguration, OutputParentCannotAdmitForeignWriters) {
    ASSERT_EQ(resolve(authored(base())), NXCONFIG_OK);
    const auto parent = temporary / "shared";
    fs::create_directory(parent);
    for (const auto writers :
         {fs::perms::group_write, fs::perms::others_write}) {
        fs::permissions(parent, fs::perms::owner_all | writers);
        const auto output = parent / "generated";
        EXPECT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
                  NXCONFIG_IO);
        EXPECT_FALSE(fs::exists(output));
        fs::remove_all(output);
    }
}

TEST_F(HostConfiguration,
       ReviewedBoardElectricalDefaultsDoNotInventConnections) {
    const auto text = "schema=2\nboard=\"stm32f407zg_qiming_v41\"\n"
                      "backend=\"baremetal\"\nclock=\"hse8-pll168\"\n"
                      "[gpio.indicator]\nbinding=\"led0\"\nmode=\"output\"\n"
                      "[gpio.button]\nbinding=\"key0\"\nmode=\"input\"\n";
    ASSERT_EQ(resolve(authored(text)), NXCONFIG_OK) << diagnostic.message;
    EXPECT_EQ(nxconfig_controller(assembly, 1U)->options.gpio.pull, 1U);
}

TEST_F(HostConfiguration, HardwareChoicesMustBeDeclaredAndMatchBinding) {
    EXPECT_EQ(resolve(authored(base() + "[hardware]\ninvented=\"fitted\"\n")),
              NXCONFIG_INVALID);
    const auto text =
        "schema=2\nboard=\"stm32f407zg_qiming_v41\"\n"
        "backend=\"baremetal\"\nclock=\"hse8-pll168\"\n"
        "[hardware]\nnor_population=\"fitted\"\ndebug_port=\"swd\"\n"
        "[spi.flash]\nbinding=\"spi1-nor\"\nmode=\"short-poll\"\n";
    /* A nonexistent binding must not resolve from a similar spelling. */
    EXPECT_NE(resolve(authored(text)), NXCONFIG_OK);
}

TEST_F(HostConfiguration, InputSymlinksAndNonRegularFilesAreRejected) {
    const auto original = authored(base());
    const auto link = temporary / "linked.toml";
    fs::create_symlink(original, link);
    EXPECT_EQ(resolve(link), NXCONFIG_IO);
    EXPECT_EQ(resolve(temporary), NXCONFIG_IO);
}

TEST_F(HostConfiguration, MemoryBudgetAndUnsupportedBackendsCannotDowngrade) {
    for (const auto& suffix :
         {"[memory]\nlibc_heap_bytes=4\n", "[memory]\nmain_stack_bytes=2050\n",
          "[memory]\nstatic_ram_limit_bytes=1024\n",
          "[memory]\nflash_load_limit_bytes=1048577\n",
          "[memory]\nmain_stack_bytes=true\n"}) {
        EXPECT_EQ(resolve(authored(base() + suffix)), NXCONFIG_INVALID);
    }
    EXPECT_EQ(
        resolve(authored("schema=2\nboard=\"stm32f407_qiming_v31\"\n"
                         "backend=\"freertos\"\nclock=\"hse8-pll168\"\n")),
        NXCONFIG_UNSUPPORTED);
}

TEST_F(HostConfiguration, SpiEndpointRateModeAndCategoryAreChecked) {
    auto text = read(root / "tests/contracts/stm32_assembly/zg-spi.toml");
    const auto board = root / "tests/contracts/stm32_assembly/zg_board";
    const auto at = text.find("board_package = \"zg_board\"");
    text.replace(at, std::string("board_package = \"zg_board\"").size(),
                 "board_package = \"" + board.string() + "\"");
    for (const auto& change :
         {std::pair{"mode = 0", "mode = 4"},
          std::pair{"controller = \"spi0\"", "controller = \"absent\""},
          std::pair{"driver = \"spi-endpoint\"", "driver = \"unknown\""}}) {
        auto invalid = text;
        invalid.replace(invalid.find(change.first), std::strlen(change.first),
                        change.second);
        EXPECT_NE(resolve(authored(invalid)), NXCONFIG_OK);
        EXPECT_EQ(assembly, nullptr);
    }
}

TEST_F(HostConfiguration,
       OutputSymlinksAndForeignFilesInOwnedBundleArePreserved) {
    ASSERT_EQ(resolve(authored(base())), NXCONFIG_OK);
    const auto output = temporary / "generated";
    const auto foreign = temporary / "foreign";
    fs::create_directory(foreign);
    write(foreign / "valuable.txt", "original");
    fs::create_directory_symlink(foreign, output);
    EXPECT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_IO);
    EXPECT_EQ(read(foreign / "valuable.txt"), "original");
    fs::remove(output);
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_OK);
    write(output / "valuable.txt", "original");
    EXPECT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_IO);
    EXPECT_EQ(read(output / "valuable.txt"), "original");
}

TEST_F(HostConfiguration, ChangedAssemblyReplacesWholeOwnedBundle) {
    const auto input =
        authored(base() + "[gpio.first]\nbinding=\"led0\"\nmode=\"output\"\n");
    ASSERT_EQ(resolve(input), NXCONFIG_OK);
    const auto output = temporary / "generated";
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_OK);
    const auto first = read(output / "nexus_instances.c");
    write(input, base() + "[gpio.second]\nbinding=\"led1\"\nmode=\"output\"\n");
    ASSERT_EQ(resolve(input), NXCONFIG_OK);
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_OK);
    const auto second = read(output / "nexus_instances.c");
    EXPECT_NE(first, second);
    EXPECT_THAT(second, HasSubstr("s_gpio_second_state"));
    EXPECT_EQ(second.find("s_gpio_first_state"), std::string::npos);
    EXPECT_THAT(read(output / "nexus_factory.h"),
                HasSubstr("NX_GPIO_ID_SECOND"));
}

int execute(const std::vector<std::string>& arguments, const fs::path& log) {
    pid_t child = fork();
    if (child == 0) {
        int descriptor = open(log.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (descriptor < 0 || dup2(descriptor, STDERR_FILENO) < 0 ||
            dup2(descriptor, STDOUT_FILENO) < 0)
            _exit(126);
        close(descriptor);
        std::vector<char*> values;
        for (const auto& argument : arguments)
            values.push_back(const_cast<char*>(argument.c_str()));
        values.push_back(nullptr);
        execvp(values[0], values.data());
        _exit(127);
    }
    if (child < 0)
        return -1;
    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
}

TEST_F(HostConfiguration,
       PublicFactoryCompilesAndRejectsCrossCategoryReferences) {
    const char* compiler = NXCONFIG_TEST_CXX;
    ASSERT_EQ(
        resolve(authored(base() +
                         "[gpio.indicator]\nbinding=\"led0\"\nmode=\"output\"\n"
                         "[uart.console]\nbinding=\"uart1\"\nmode=\"irq\"\n")),
        NXCONFIG_OK);
    const auto output = temporary / "generated";
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_OK);
    const auto source = temporary / "consumer.cpp";
    const auto log = temporary / "compiler.log";
    write(source, "#include \"nexus_factory_strong.h\"\n"
                  "const nx_uart_port_t* consumer() {\n"
                  "return nx_factory_uart_ref(NX_UART_REF_CONSOLE);\n}\n");
    const std::vector<std::string> arguments{
        compiler,
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-pedantic",
        "-I" + output.string(),
        "-I" + (root / "io/include").string(),
        "-c",
        source.string(),
        "-o",
        (temporary / "consumer.o").string()};
    ASSERT_EQ(execute(arguments, log), 0) << read(log);
    write(source, "#include \"nexus_factory_strong.h\"\n"
                  "const nx_uart_port_t* consumer() {\n"
                  "return nx_factory_uart_ref(NX_GPIO_REF_INDICATOR);\n}\n");
    EXPECT_NE(execute(arguments, log), 0);
    EXPECT_THAT(read(log), HasSubstr("nx_gpio_ref_t"));
}

TEST_F(HostConfiguration, GeneratedObjectsCompileAgainstSelectedOfficialSdk) {
    const char* compiler = std::getenv("NXCONFIG_TEST_ARM_CC");
    if (compiler == nullptr)
        compiler = NXCONFIG_TEST_ARM_CC_DEFAULT;
    for (const auto& fixture :
         {root / "tools/configure/assemblies/qiming-baremetal.toml",
          root / "tests/contracts/stm32_assembly/zg-spi.toml"}) {
        ASSERT_EQ(resolve(fixture), NXCONFIG_OK);
        const auto output = temporary / "generated";
        ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
                  NXCONFIG_OK);
        std::vector<std::string> arguments{compiler,
                                           "-std=c11",
                                           "-mcpu=cortex-m4",
                                           "-mthumb",
                                           "-mfloat-abi=hard",
                                           "-mfpu=fpv4-sp-d16",
                                           "-Wall",
                                           "-Wextra",
                                           "-Werror",
                                           "-DSTM32F407xx",
                                           "-I" + output.string()};
        for (const auto& include :
             {"core/include", "arch/include", "os/include", "io/include",
              "soc/stm32/stm32f4/private", "vendors/st/cmsis_device_f4/Include",
              "vendors/arm/CMSIS_6/CMSIS/Core/Include"})
            arguments.push_back("-I" + (root / include).string());
        arguments.insert(arguments.end(),
                         {"-c", (output / "nexus_instances.c").string(), "-o",
                          (temporary / "instances.o").string()});
        const auto log = temporary / "arm-compiler.log";
        EXPECT_EQ(execute(arguments, log), 0) << read(log);
    }
}

TEST_F(HostConfiguration,
       LifecycleUsesHandwrittenAdaptersAndNoParallelRegistry) {
    ASSERT_EQ(
        resolve(root / "tools/configure/assemblies/qiming-baremetal.toml"),
        NXCONFIG_OK);
    const auto output = temporary / "generated";
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_OK);
    const auto source = read(output / "nexus_instances.c");
    EXPECT_THAT(
        source,
        HasSubstr("const nx_lifecycle_plan_t nx_generated_lifecycle_plan"));
    EXPECT_THAT(source, HasSubstr("nx_stm32f4_pins_lifecycle_ops"));
    EXPECT_THAT(source, HasSubstr("nx_stm32f4_gpio_lifecycle_ops"));
    EXPECT_THAT(source, HasSubstr("nx_stm32f4_uart_irq_lifecycle_ops"));
    EXPECT_EQ(source.find("nx_generated_uart_binding_t"), std::string::npos);
    EXPECT_EQ(source.find("nx_stm32_uart_byte_cold_prepare("),
              std::string::npos);
}

TEST(HostInputIdentity, MatchesStandardSha256VectorsAcrossBlockBoundaries) {
    for (const auto& vector :
         {std::pair{"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca49599"
                        "1b7852b855"},
          std::pair{"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb41"
                           "0ff61f20015ad"},
          std::pair{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419"
                    "db06c1"}}) {
        char digest[65];
        nxconfig_sha256(vector.first, std::strlen(vector.first), digest);
        EXPECT_STREQ(digest, vector.second);
    }
    const std::string million(1000000U, 'a');
    char digest[65];
    nxconfig_sha256(million.data(), million.size(), digest);
    EXPECT_STREQ(
        digest,
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_F(HostConfiguration, PollingSpiRequiresAnExplicitAddressedEndpoint) {
    const auto fixture = root / "tests/contracts/stm32_assembly/zg-spi.toml";
    auto text = read(fixture);
    text.erase(text.find("[spi_device.sensor0]"));
    const auto board = root / "tests/contracts/stm32_assembly/zg_board";
    const auto at = text.find("board_package = \"zg_board\"");
    text.replace(at, std::string("board_package = \"zg_board\"").size(),
                 "board_package = \"" + board.string() + "\"");
    EXPECT_EQ(resolve(authored(text)), NXCONFIG_INVALID);
}

TEST_F(HostConfiguration, MissingParserLockEntryCannotBypassIntegrity) {
    const auto source = temporary / "source";
    fs::create_directories(source / "tools");
    fs::copy(root / "tools/nxconfig", source / "tools/nxconfig",
             fs::copy_options::recursive);
    fs::create_directories(source / "dependencies");
    for (const auto& parser : {"tomlc99", "cjson"}) {
        fs::copy(root / "dependencies" / (std::string(parser) + ".lock.json"),
                 source / "dependencies" /
                     (std::string(parser) + ".lock.json"));
        fs::create_directories(source / "ext" / parser);
    }
    for (const auto& file : {"toml.c", "toml.h", "LICENSE"})
        fs::copy(root / "ext/tomlc99" / file, source / "ext/tomlc99" / file);
    for (const auto& file : {"cJSON.c", "cJSON.h", "LICENSE"})
        fs::copy(root / "ext/cjson" / file, source / "ext/cjson" / file);
    auto lock_text = read(source / "dependencies/tomlc99.lock.json");
    cJSON* lock = cJSON_Parse(lock_text.c_str());
    ASSERT_NE(lock, nullptr);
    cJSON_DeleteItemFromObjectCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(lock, "files"), "toml.c");
    char* encoded = cJSON_Print(lock);
    ASSERT_NE(encoded, nullptr);
    write(source / "dependencies/tomlc99.lock.json", encoded);
    cJSON_free(encoded);
    cJSON_Delete(lock);
    const auto log = temporary / "cmake.log";
    EXPECT_NE(
        execute({NXCONFIG_TEST_CMAKE, "-S",
                 (source / "tools/nxconfig").string(), "-B",
                 (temporary / "build").string(), "-G", "Ninja",
                 std::string("-DCMAKE_MAKE_PROGRAM=") + NXCONFIG_TEST_NINJA},
                log),
        0);
    EXPECT_THAT(read(log), HasSubstr("exact parser input set"));
}

TEST_F(HostConfiguration, UartFactsMustProvideTheCompleteTwoPadColdContract) {
    const auto original = root;
    root = temporary / "source";
    fs::create_directories(root / "soc/stm32/stm32f4");
    for (const auto& name : {"soc.json", "routes.json", "devices.json"})
        fs::copy(original / "soc/stm32/stm32f4" / name,
                 root / "soc/stm32/stm32f4" / name);
    fs::create_directories(root / "boards/stm32f407_qiming_v31");
    fs::copy(original / "boards/stm32f407_qiming_v31/board.json",
             root / "boards/stm32f407_qiming_v31/board.json");
    auto text = read(root / "soc/stm32/stm32f4/routes.json");
    cJSON* routes = cJSON_Parse(text.c_str());
    ASSERT_NE(routes, nullptr);
    cJSON* item;
    cJSON_ArrayForEach(item,
                       cJSON_GetObjectItemCaseSensitive(routes, "routes")) {
        cJSON* id = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (cJSON_IsString(id) &&
            std::strcmp(id->valuestring, "usart1-pa9-pa10") == 0)
            cJSON_DeleteItemFromArray(
                cJSON_GetObjectItemCaseSensitive(item, "pins"), 1);
    }
    char* encoded = cJSON_Print(routes);
    ASSERT_NE(encoded, nullptr);
    write(root / "soc/stm32/stm32f4/routes.json", encoded);
    cJSON_free(encoded);
    cJSON_Delete(routes);
    EXPECT_EQ(
        resolve(authored(base() +
                         "[uart.console]\nbinding=\"uart1\"\nmode=\"irq\"\n")),
        NXCONFIG_INVALID);
}

TEST_F(HostConfiguration, OverlongOutputCannotTruncateOrReplaceOwnedFiles) {
    ASSERT_EQ(resolve(authored(base())), NXCONFIG_OK);
    const auto output = temporary / "generated";
    ASSERT_EQ(nxconfig_emit(assembly, output.c_str(), &diagnostic),
              NXCONFIG_OK);
    const auto manifest = read(output / "manifest.json");
    const auto overlong = output.string() + "/" + std::string(4096U, 'a');
    EXPECT_EQ(nxconfig_emit(assembly, overlong.c_str(), &diagnostic),
              NXCONFIG_IO);
    EXPECT_EQ(read(output / "manifest.json"), manifest);
    EXPECT_TRUE(fs::is_regular_file(output / ".nxconfig-owner"));
}
} /* namespace */
