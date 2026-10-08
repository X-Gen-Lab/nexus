foreach(board stm32f407_qiming_v31 stm32f407ve_sky_qingchun)
    add_executable(test_board_${board} test_stm32_board_resources.c
        "${NEXUS_SOURCE_DIR}/boards/${board}/board.c"
        "${NEXUS_SOURCE_DIR}/boards/${board}/uart.c")
    target_include_directories(test_board_${board} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/fake"
        "${NEXUS_SOURCE_DIR}/boards/${board}"
        "${NEXUS_SOURCE_DIR}/hal/include"
        "${NEXUS_SOURCE_DIR}/platforms/stm32/src/hal/uart")
    if(board STREQUAL "stm32f407_qiming_v31")
        target_compile_definitions(test_board_${board} PRIVATE TEST_QIMING_BOARD=1)
    endif()
    target_compile_features(test_board_${board} PRIVATE c_std_11)
    target_link_libraries(test_board_${board} PRIVATE nexus_build_options)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_board_${board} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME board_${board} COMMAND test_board_${board})
    set_tests_properties(board_${board} PROPERTIES TIMEOUT 10 LABELS "board;host-fake;contract")
endforeach()
