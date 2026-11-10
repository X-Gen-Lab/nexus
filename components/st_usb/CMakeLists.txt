# Optional locked middleware; functions do not prepare unselected suppliers.
function(nexus_add_st_usb_host)
    if(TARGET nexus_st_usb_host)
        return()
    endif()
    get_filename_component(source "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.."
        ABSOLUTE)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${source}/tools/maintenance/st_usb_lock.py" --root "${source}"
        --role host RESULT_VARIABLE result OUTPUT_VARIABLE identity
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Selected ST Host identity failed: ${error}")
    endif()
    string(JSON count LENGTH "${identity}" configure_inputs)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON path GET "${identity}" configure_inputs ${index})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    endforeach()
    set(private "${source}/components/st_usb/private")
    set(vendor "${source}/vendors/st/usb_host/Core")
    add_library(nexus_st_usb_host_core STATIC
        "${vendor}/Src/usbh_core.c" "${vendor}/Src/usbh_ctlreq.c"
        "${vendor}/Src/usbh_ioreq.c" "${vendor}/Src/usbh_pipes.c")
    target_include_directories(nexus_st_usb_host_core PRIVATE "${private}")
    target_include_directories(nexus_st_usb_host_core SYSTEM PRIVATE "${vendor}/Inc")
    target_compile_features(nexus_st_usb_host_core PRIVATE c_std_11)
    set_target_properties(nexus_st_usb_host_core PROPERTIES C_EXTENSIONS OFF)
    target_link_libraries(nexus_st_usb_host_core PRIVATE nexus_build_options)
    add_library(nexus_st_usb_host STATIC
        "${source}/components/st_usb/src/usb_host_stack.c")
    target_include_directories(nexus_st_usb_host PRIVATE "${private}")
    target_include_directories(nexus_st_usb_host SYSTEM PRIVATE "${vendor}/Inc")
    target_link_libraries(nexus_st_usb_host PUBLIC Nexus::IOUSBHost
        PRIVATE nexus_st_usb_host_core Nexus::Arch nexus_owned_warnings
            nexus_build_options)
    add_library(Nexus::STUSBHost ALIAS nexus_st_usb_host)
endfunction()
