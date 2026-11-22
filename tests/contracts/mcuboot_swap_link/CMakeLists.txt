# Keep source and installed SDK consumers on the same authored reference.
# Author: Nexus Team
cmake_minimum_required(VERSION 3.31)
project(NexusMCUbootScratchSourceReference C ASM)
if(NOT DEFINED NEXUS_PLATFORM_ROOT)
    message(FATAL_ERROR "Explicit Nexus source/SDK root is required")
endif()
include(
    "${NEXUS_PLATFORM_ROOT}/cmake/package/verification/mcuboot_swap/CMakeLists.txt"
)
