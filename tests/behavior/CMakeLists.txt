# Host behavior tests use the ordinary platform targets and pinned framework.
# Author: Nexus Team
if(CMAKE_CROSSCOMPILING OR NOT NEXUS_SOC_FAMILY STREQUAL "native")
    message(FATAL_ERROR "GoogleTest/GoogleMock require the Native test profile")
endif()
include("${NEXUS_SOURCE_DIR}/tests/cmake/GoogleTest.cmake")
include("${NEXUS_SOURCE_DIR}/tests/cmake/CMSISHostModel.cmake")
nexus_host_google_test()
set(nexus_google_root "${CMAKE_CURRENT_SOURCE_DIR}")
include("${NEXUS_SOURCE_DIR}/tests/cmake/BehaviorTest.cmake")
add_custom_target(nexus_google_contracts)

# Include domain entries in one scope so model source properties stay explicit.
foreach(
    nexus_behavior_domain
    core
    arch
    components
    io
    os
)
    include("${nexus_google_root}/${nexus_behavior_domain}/tests.cmake")
endforeach()
set(nexus_behavior_domain soc)
include("${nexus_google_root}/soc/stm32/stm32f4/tests.cmake")
include("${nexus_google_root}/soc/stm32/stm32f4/shared_models.cmake")
include("${nexus_google_root}/soc/stm32/stm32f1/tests.cmake")
include("${NEXUS_SOURCE_DIR}/tests/cmake/GooglePlan.cmake")
