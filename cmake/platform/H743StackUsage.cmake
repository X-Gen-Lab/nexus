# Ninja models OBJECT_OUTPUTS as phony dependents of the compiler object.
# A missing .su therefore needs a restat witness to dirty the original CMake
# compiler rule. Never reproduce a compile command in this recovery check.
if(NOT DEFINED NEXUS_STACK_WITNESS OR NOT NEXUS_STACK_FILES)
    message(FATAL_ERROR "Missing H743 stack-usage recovery inputs")
endif()
set(refresh FALSE)
if(NOT EXISTS "${NEXUS_STACK_WITNESS}")
    set(refresh TRUE)
endif()
foreach(file IN LISTS NEXUS_STACK_FILES)
    if(NOT EXISTS "${file}")
        set(refresh TRUE)
    endif()
endforeach()
if(refresh)
    file(TOUCH "${NEXUS_STACK_WITNESS}")
endif()
