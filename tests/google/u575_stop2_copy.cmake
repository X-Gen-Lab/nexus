# Keep the coupled RTC/LPDMA instruction and MMIO ports local to one directory.
# Author: Nexus Team
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/u575_stop2_copy"
    "${CMAKE_CURRENT_BINARY_DIR}/u575_stop2_copy")
