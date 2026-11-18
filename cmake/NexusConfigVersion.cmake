# Version selection reads declarative policy only; discovery validates identity.
file(
    READ "${CMAKE_CURRENT_LIST_DIR}/../dependencies/platform-compatibility.json"
    policy
)
string(JSON PACKAGE_VERSION GET "${policy}" semantic_version)
set(PACKAGE_VERSION_COMPATIBLE FALSE)
set(PACKAGE_VERSION_EXACT FALSE)
if(
    NOT PACKAGE_FIND_VERSION_RANGE
    AND PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION
)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    set(PACKAGE_VERSION_EXACT TRUE)
endif()
