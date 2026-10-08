if(NOT CONFIG_PRODUCT_NAME OR NOT CONFIG_BOARD_NAME)
    message(FATAL_ERROR "An implemented board and product profile must be selected")
endif()
add_library(nexus_product_runtime STATIC src/product.c)
target_include_directories(nexus_product_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_product_runtime PUBLIC hal osal)

# A product is the single firmware composition target. Controller objects and
# startup are injected by the selected platform, while optional services remain
# explicit application dependencies instead of being forced into every image.
add_library(nexus_product INTERFACE)
target_link_libraries(nexus_product INTERFACE
    "platform_${NEXUS_PLATFORM}"
    "$<TARGET_OBJECTS:platform_${NEXUS_PLATFORM}>" nexus_product_runtime)
add_library(Nexus::Product ALIAS nexus_product)
