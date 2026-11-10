# Explicit selected middleware; unselected Device Core remains unprepared.
function(nexus_add_st_usb_device)
    if(TARGET nexus_st_usb_device)
        return()
    endif()
    get_filename_component(source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.."
        ABSOLUTE)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${source}/tools/maintenance/st_usb_lock.py" --root "${source}"
        --role device RESULT_VARIABLE result OUTPUT_VARIABLE identity
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Selected ST Device identity failed: ${error}")
    endif()
    string(JSON count LENGTH "${identity}" configure_inputs)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON path GET "${identity}" configure_inputs ${index})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    endforeach()
    set(private "${source}/components/st_usb_device/private")
    set(vendor "${source}/vendors/st/usb_device/Core")
    add_library(nexus_st_usb_device_core STATIC
        "${vendor}/Src/usbd_core.c" "${vendor}/Src/usbd_ctlreq.c"
        "${vendor}/Src/usbd_ioreq.c")
    target_include_directories(nexus_st_usb_device_core PRIVATE "${private}")
    target_include_directories(nexus_st_usb_device_core SYSTEM PRIVATE "${vendor}/Inc")
    target_compile_features(nexus_st_usb_device_core PRIVATE c_std_11)
    set_target_properties(nexus_st_usb_device_core PROPERTIES C_EXTENSIONS OFF)
    target_link_libraries(nexus_st_usb_device_core PRIVATE nexus_build_options)
    add_library(nexus_st_usb_device STATIC
        "${source}/components/st_usb_device/src/usb_device_stack.c")
    target_include_directories(nexus_st_usb_device PRIVATE "${private}")
    target_include_directories(nexus_st_usb_device SYSTEM PRIVATE "${vendor}/Inc")
    target_link_libraries(nexus_st_usb_device PUBLIC Nexus::IOUSBDevice
        PRIVATE nexus_st_usb_device_core Nexus::Arch nexus_owned_warnings
            nexus_build_options)
    add_library(Nexus::STUSBDevice ALIAS nexus_st_usb_device)
endfunction()
