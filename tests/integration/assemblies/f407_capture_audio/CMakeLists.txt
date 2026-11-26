cmake_minimum_required(VERSION 3.31)
project(f407_capture_audio_fixture LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT NEXUS_SOURCE_ROOT OR NOT NEXUS_ASSEMBLY_FILE)
    message(
        FATAL_ERROR
        "Capture/audio fixture needs an explicit source and TOML"
    )
endif()
find_package(
    Nexus
    CONFIG
    REQUIRED
    PATHS "${NEXUS_SOURCE_ROOT}/cmake"
    NO_DEFAULT_PATH
)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_controller_modes(capture_modes capture)
set(consumer controller_main.c)
if("${capture_modes}" MATCHES "frames-")
    set(consumer frame_session.c)
endif()
nexus_add_firmware(f407_capture_audio_fixture SOURCES "${consumer}")
nexus_target_warnings(f407_capture_audio_fixture STRICT)
