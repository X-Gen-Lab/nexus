# This fixture lives outside the SDK board roots when the regression runs.
add_library(fixture_external_board INTERFACE)
target_include_directories(fixture_external_board INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fixture_external_board INTERFACE Nexus::Config)
