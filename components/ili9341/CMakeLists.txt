# Explicit RGB565 display component; no panel, bus or framebuffer is implicit.
add_library(nexus_ili9341 STATIC EXCLUDE_FROM_ALL src/ili9341.c)
nexus_apply_target_policy(nexus_ili9341)
target_include_directories(nexus_ili9341 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_ili9341 PUBLIC Nexus::Core Nexus::IOParallel16
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::ILI9341 ALIAS nexus_ili9341)
