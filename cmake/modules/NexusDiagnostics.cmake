##############################################################################
# NexusDiagnostics.cmake - Error Handling and Diagnostics Module
##############################################################################
#
# NexusDiagnostics.cmake
# Error handling, logging, and diagnostics for Nexus build
# Author: Nexus Team
#
# This module provides comprehensive error handling, logging, and diagnostic
# capabilities for the Nexus build system.
#
##############################################################################

include_guard(GLOBAL)

##############################################################################
# Global Variables
##############################################################################

# Log level definitions
set(NEXUS_LOG_LEVEL_DEBUG 0)
set(NEXUS_LOG_LEVEL_INFO 1)
set(NEXUS_LOG_LEVEL_WARNING 2)
set(NEXUS_LOG_LEVEL_ERROR 3)

# Default log level
if(NOT DEFINED NEXUS_LOG_LEVEL)
    set(NEXUS_LOG_LEVEL ${NEXUS_LOG_LEVEL_INFO} CACHE STRING "Nexus log level")
endif()

# Log file path
if(NOT DEFINED NEXUS_LOG_FILE)
    set(NEXUS_LOG_FILE "${CMAKE_BINARY_DIR}/nexus_build.log" CACHE FILEPATH "Nexus log file")
endif()

# Structured log file (JSON)
if(NOT DEFINED NEXUS_STRUCTURED_LOG_FILE)
    set(NEXUS_STRUCTURED_LOG_FILE "${CMAKE_BINARY_DIR}/nexus_build.json" CACHE FILEPATH "Nexus structured log file")
endif()

# Build report file
if(NOT DEFINED NEXUS_BUILD_REPORT_FILE)
    set(NEXUS_BUILD_REPORT_FILE "${CMAKE_BINARY_DIR}/nexus_build_report.txt" CACHE FILEPATH "Nexus build report file")
endif()

# Build statistics
set(NEXUS_BUILD_START_TIME "" CACHE INTERNAL "Build start time")
set(NEXUS_BUILD_ERROR_COUNT 0 CACHE INTERNAL "Build error count")
set(NEXUS_BUILD_WARNING_COUNT 0 CACHE INTERNAL "Build warning count")
set(NEXUS_BUILD_FILE_COUNT 0 CACHE INTERNAL "Build file count")

##############################################################################
# Error Message Formatting
##############################################################################

#
# Parse compiler error message
# ERROR_TEXT: Raw error text from compiler
# OUT_FILE: Output variable for file path
# OUT_LINE: Output variable for line number
# OUT_COLUMN: Output variable for column number
# OUT_MESSAGE: Output variable for error message
# Returns: TRUE if parsing succeeded, FALSE otherwise
#
function(nexus_parse_compiler_error ERROR_TEXT OUT_FILE OUT_LINE OUT_COLUMN OUT_MESSAGE)
    # GCC/Clang format: file:line:column: error: message
    string(REGEX MATCH "([^:]+):([0-9]+):([0-9]+): (error|warning): (.*)"
           MATCH_RESULT "${ERROR_TEXT}")

    if(MATCH_RESULT)
        set(${OUT_FILE} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        set(${OUT_LINE} "${CMAKE_MATCH_2}" PARENT_SCOPE)
        set(${OUT_COLUMN} "${CMAKE_MATCH_3}" PARENT_SCOPE)
        set(${OUT_MESSAGE} "${CMAKE_MATCH_5}" PARENT_SCOPE)
        return()
    endif()

    # MSVC format: file(line): error C####: message
    string(REGEX MATCH "([^(]+)\\(([0-9]+)\\): (error|warning) [^:]+: (.*)"
           MATCH_RESULT "${ERROR_TEXT}")

    if(MATCH_RESULT)
        set(${OUT_FILE} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        set(${OUT_LINE} "${CMAKE_MATCH_2}" PARENT_SCOPE)
        set(${OUT_COLUMN} "0" PARENT_SCOPE)
        set(${OUT_MESSAGE} "${CMAKE_MATCH_4}" PARENT_SCOPE)
        return()
    endif()

    # IAR format: "file",line  Error[code]: message
    string(REGEX MATCH "\"([^\"]+)\",([0-9]+)  (Error|Warning)\\[[^]]+\\]: (.*)"
           MATCH_RESULT "${ERROR_TEXT}")

    if(MATCH_RESULT)
        set(${OUT_FILE} "${CMAKE_MATCH_1}" PARENT_SCOPE)
        set(${OUT_LINE} "${CMAKE_MATCH_2}" PARENT_SCOPE)
        set(${OUT_COLUMN} "0" PARENT_SCOPE)
        set(${OUT_MESSAGE} "${CMAKE_MATCH_4}" PARENT_SCOPE)
        return()
    endif()

    # Could not parse
    set(${OUT_FILE} "" PARENT_SCOPE)
    set(${OUT_LINE} "0" PARENT_SCOPE)
    set(${OUT_COLUMN} "0" PARENT_SCOPE)
    set(${OUT_MESSAGE} "${ERROR_TEXT}" PARENT_SCOPE)
endfunction()

#
# Format error message with context
# FILE: Source file path
# LINE: Line number
# COLUMN: Column number
# MESSAGE: Error message
# Returns: Formatted error message
#
function(nexus_format_error_message FILE LINE COLUMN MESSAGE)
    set(FORMATTED_MESSAGE "")

    # Header
    string(APPEND FORMATTED_MESSAGE "\n")
    string(APPEND FORMATTED_MESSAGE "================================================================================\n")
    string(APPEND FORMATTED_MESSAGE "BUILD ERROR\n")
    string(APPEND FORMATTED_MESSAGE "================================================================================\n")

    # Location
    string(APPEND FORMATTED_MESSAGE "File:    ${FILE}\n")
    string(APPEND FORMATTED_MESSAGE "Line:    ${LINE}")
    if(NOT COLUMN EQUAL 0)
        string(APPEND FORMATTED_MESSAGE ":${COLUMN}")
    endif()
    string(APPEND FORMATTED_MESSAGE "\n")

    # Message
    string(APPEND FORMATTED_MESSAGE "Error:   ${MESSAGE}\n")

    # Footer
    string(APPEND FORMATTED_MESSAGE "================================================================================\n")

    message(FATAL_ERROR "${FORMATTED_MESSAGE}")
endfunction()

##############################################################################
# Error Context Extraction
##############################################################################

#
# Extract code context around error line
# FILE: Source file path
# LINE: Line number
# CONTEXT_LINES: Number of context lines (default: 3)
# Returns: Code context string
#
function(nexus_extract_error_context FILE LINE CONTEXT_LINES)
    if(NOT DEFINED CONTEXT_LINES)
        set(CONTEXT_LINES 3)
    endif()

    # Check if file exists
    if(NOT EXISTS "${FILE}")
        message(WARNING "Cannot extract context: file not found: ${FILE}")
        return()
    endif()

    # Read file
    file(READ "${FILE}" FILE_CONTENT)

    # Split into lines
    string(REPLACE "\n" ";" FILE_LINES "${FILE_CONTENT}")
    list(LENGTH FILE_LINES TOTAL_LINES)

    # Calculate context range
    math(EXPR START_LINE "${LINE} - ${CONTEXT_LINES}")
    math(EXPR END_LINE "${LINE} + ${CONTEXT_LINES}")

    if(START_LINE LESS 1)
        set(START_LINE 1)
    endif()

    if(END_LINE GREATER TOTAL_LINES)
        set(END_LINE ${TOTAL_LINES})
    endif()

    # Extract context
    set(CONTEXT "")
    math(EXPR CURRENT_LINE "${START_LINE}")

    while(CURRENT_LINE LESS_EQUAL END_LINE)
        math(EXPR LIST_INDEX "${CURRENT_LINE} - 1")
        list(GET FILE_LINES ${LIST_INDEX} LINE_CONTENT)

        # Format line number
        string(LENGTH "${END_LINE}" MAX_WIDTH)
        string(LENGTH "${CURRENT_LINE}" CURRENT_WIDTH)
        math(EXPR PADDING "${MAX_WIDTH} - ${CURRENT_WIDTH}")

        set(LINE_PREFIX "")
        foreach(i RANGE ${PADDING})
            string(APPEND LINE_PREFIX " ")
        endforeach()

        # Mark error line
        if(CURRENT_LINE EQUAL LINE)
            string(APPEND CONTEXT "${LINE_PREFIX}${CURRENT_LINE} > ${LINE_CONTENT}\n")
        else()
            string(APPEND CONTEXT "${LINE_PREFIX}${CURRENT_LINE} | ${LINE_CONTENT}\n")
        endif()

        math(EXPR CURRENT_LINE "${CURRENT_LINE} + 1")
    endwhile()

    # Return context
    set(NEXUS_ERROR_CONTEXT "${CONTEXT}" PARENT_SCOPE)
endfunction()

#
# Provide error fix suggestions
# ERROR_MESSAGE: Error message
# Returns: Fix suggestion string
#
function(nexus_suggest_error_fix ERROR_MESSAGE)
    set(SUGGESTION "")

    # Common error patterns and suggestions
    if(ERROR_MESSAGE MATCHES "undefined reference")
        set(SUGGESTION "Check if the function is declared and linked correctly.\nMake sure all required libraries are linked.")
    elseif(ERROR_MESSAGE MATCHES "implicit declaration")
        set(SUGGESTION "Include the appropriate header file that declares this function.")
    elseif(ERROR_MESSAGE MATCHES "undeclared identifier")
        set(SUGGESTION "Check if the identifier is declared in scope.\nVerify header includes and spelling.")
    elseif(ERROR_MESSAGE MATCHES "expected.*before")
        set(SUGGESTION "Check for missing semicolons, braces, or parentheses.")
    elseif(ERROR_MESSAGE MATCHES "conflicting types")
        set(SUGGESTION "Check for inconsistent function declarations.\nEnsure declaration and definition match.")
    elseif(ERROR_MESSAGE MATCHES "multiple definition")
        set(SUGGESTION "Check for duplicate symbol definitions.\nUse 'static' or 'inline' for internal functions.")
    else()
        set(SUGGESTION "Review the error message and check the code at the indicated location.")
    endif()

    set(NEXUS_ERROR_SUGGESTION "${SUGGESTION}" PARENT_SCOPE)
endfunction()

##############################################################################
# Logging System
##############################################################################

#
# Initialize logging system
#
function(nexus_init_logging)
    # Create log file
    file(WRITE "${NEXUS_LOG_FILE}" "")
    file(WRITE "${NEXUS_STRUCTURED_LOG_FILE}" "[\n")

    # Record start time
    string(TIMESTAMP START_TIME "%Y-%m-%dT%H:%M:%S")
    set(NEXUS_BUILD_START_TIME "${START_TIME}" CACHE INTERNAL "Build start time")

    # Log initialization
    nexus_log(INFO "Nexus build system initialized")
    nexus_log(INFO "CMake version: ${CMAKE_VERSION}")
    nexus_log(INFO "Platform: ${CMAKE_SYSTEM_NAME}")
    nexus_log(INFO "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
endfunction()

#
# Log message with level
# LEVEL: Log level (DEBUG, INFO, WARNING, ERROR)
# MESSAGE: Log message
#
function(nexus_log LEVEL MESSAGE)
    # Get numeric level
    set(NUMERIC_LEVEL ${NEXUS_LOG_LEVEL_${LEVEL}})

    # Check if should log
    if(NUMERIC_LEVEL LESS NEXUS_LOG_LEVEL)
        return()
    endif()

    # Get timestamp
    string(TIMESTAMP TIMESTAMP "%Y-%m-%dT%H:%M:%S")

    # Format message
    set(LOG_MESSAGE "[${TIMESTAMP}] [${LEVEL}] ${MESSAGE}")

    # Write to log file
    file(APPEND "${NEXUS_LOG_FILE}" "${LOG_MESSAGE}\n")

    # Write structured log entry
    nexus_write_structured_log("${LEVEL}" "${MESSAGE}")

    # Print to console based on level
    if(LEVEL STREQUAL "ERROR")
        message(SEND_ERROR "${LOG_MESSAGE}")
        math(EXPR ERROR_COUNT "${NEXUS_BUILD_ERROR_COUNT} + 1")
        set(NEXUS_BUILD_ERROR_COUNT ${ERROR_COUNT} CACHE INTERNAL "Build error count")
    elseif(LEVEL STREQUAL "WARNING")
        message(WARNING "${LOG_MESSAGE}")
        math(EXPR WARNING_COUNT "${NEXUS_BUILD_WARNING_COUNT} + 1")
        set(NEXUS_BUILD_WARNING_COUNT ${WARNING_COUNT} CACHE INTERNAL "Build warning count")
    elseif(LEVEL STREQUAL "INFO")
        message(STATUS "${LOG_MESSAGE}")
    elseif(LEVEL STREQUAL "DEBUG")
        message(VERBOSE "${LOG_MESSAGE}")
    endif()
endfunction()

#
# Write structured log entry (JSON)
# LEVEL: Log level
# MESSAGE: Log message
#
function(nexus_write_structured_log LEVEL MESSAGE)
    # Get timestamp
    string(TIMESTAMP TIMESTAMP "%Y-%m-%dT%H:%M:%S")

    # Escape JSON special characters
    string(REPLACE "\\" "\\\\" MESSAGE_ESCAPED "${MESSAGE}")
    string(REPLACE "\"" "\\\"" MESSAGE_ESCAPED "${MESSAGE_ESCAPED}")
    string(REPLACE "\n" "\\n" MESSAGE_ESCAPED "${MESSAGE_ESCAPED}")

    # Create JSON entry
    set(JSON_ENTRY "  {\n")
    string(APPEND JSON_ENTRY "    \"timestamp\": \"${TIMESTAMP}\",\n")
    string(APPEND JSON_ENTRY "    \"level\": \"${LEVEL}\",\n")
    string(APPEND JSON_ENTRY "    \"message\": \"${MESSAGE_ESCAPED}\"\n")
    string(APPEND JSON_ENTRY "  },\n")

    # Append to structured log file
    file(APPEND "${NEXUS_STRUCTURED_LOG_FILE}" "${JSON_ENTRY}")
endfunction()

#
# Finalize logging system
#
function(nexus_finalize_logging)
    # Close JSON array
    file(APPEND "${NEXUS_STRUCTURED_LOG_FILE}" "]\n")

    nexus_log(INFO "Nexus build system finalized")
endfunction()

##############################################################################
# Build Report Generation
##############################################################################

#
# Generate build report
# FORMAT: Report format (TEXT, HTML, JSON)
#
function(nexus_generate_build_report)
    cmake_parse_arguments(
        ARG
        ""
        "FORMAT"
        ""
        ${ARGN}
    )

    # Default format
    if(NOT ARG_FORMAT)
        set(ARG_FORMAT "TEXT")
    endif()

    # Calculate build time
    string(TIMESTAMP END_TIME "%Y-%m-%dT%H:%M:%S")

    # Generate report based on format
    if(ARG_FORMAT STREQUAL "TEXT")
        nexus_generate_text_report()
    elseif(ARG_FORMAT STREQUAL "HTML")
        nexus_generate_html_report()
    elseif(ARG_FORMAT STREQUAL "JSON")
        nexus_generate_json_report()
    else()
        message(WARNING "Unknown report format: ${ARG_FORMAT}")
    endif()
endfunction()

#
# Generate text format build report
#
function(nexus_generate_text_report)
    set(REPORT "")

    # Header
    string(APPEND REPORT "================================================================================\n")
    string(APPEND REPORT "NEXUS BUILD REPORT\n")
    string(APPEND REPORT "================================================================================\n\n")

    # Build information
    string(APPEND REPORT "Build Configuration:\n")
    string(APPEND REPORT "  Platform:        ${CMAKE_SYSTEM_NAME}\n")
    string(APPEND REPORT "  Compiler:        ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}\n")
    string(APPEND REPORT "  Build Type:      ${CMAKE_BUILD_TYPE}\n")
    string(APPEND REPORT "  CMake Version:   ${CMAKE_VERSION}\n\n")

    # Build statistics
    string(APPEND REPORT "Build Statistics:\n")
    string(APPEND REPORT "  Start Time:      ${NEXUS_BUILD_START_TIME}\n")
    string(TIMESTAMP END_TIME "%Y-%m-%dT%H:%M:%S")
    string(APPEND REPORT "  End Time:        ${END_TIME}\n")
    string(APPEND REPORT "  Files Compiled:  ${NEXUS_BUILD_FILE_COUNT}\n")
    string(APPEND REPORT "  Errors:          ${NEXUS_BUILD_ERROR_COUNT}\n")
    string(APPEND REPORT "  Warnings:        ${NEXUS_BUILD_WARNING_COUNT}\n\n")

    # Build status
    if(NEXUS_BUILD_ERROR_COUNT GREATER 0)
        string(APPEND REPORT "Build Status:      FAILED\n")
    else()
        string(APPEND REPORT "Build Status:      SUCCESS\n")
    endif()

    # Footer
    string(APPEND REPORT "\n================================================================================\n")

    # Write report
    file(WRITE "${NEXUS_BUILD_REPORT_FILE}" "${REPORT}")

    message(STATUS "Build report generated: ${NEXUS_BUILD_REPORT_FILE}")
endfunction()

#
# Generate HTML format build report
#
function(nexus_generate_html_report)
    # Use configured directory or default to CMAKE_BINARY_DIR
    if(DEFINED NEXUS_REPORT_DIR)
        set(REPORT_FILE "${NEXUS_REPORT_DIR}/nexus_build_report.html")
    else()
        set(REPORT_FILE "${CMAKE_BINARY_DIR}/nexus_build_report.html")
    endif()

    set(HTML "<!DOCTYPE html>\n")
    string(APPEND HTML "<html>\n<head>\n")
    string(APPEND HTML "<title>Nexus Build Report</title>\n")
    string(APPEND HTML "<style>\n")
    string(APPEND HTML "body { font-family: Arial, sans-serif; margin: 20px; }\n")
    string(APPEND HTML "h1 { color: #333; }\n")
    string(APPEND HTML "table { border-collapse: collapse; width: 100%; }\n")
    string(APPEND HTML "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n")
    string(APPEND HTML "th { background-color: #4CAF50; color: white; }\n")
    string(APPEND HTML ".success { color: green; font-weight: bold; }\n")
    string(APPEND HTML ".failed { color: red; font-weight: bold; }\n")
    string(APPEND HTML "</style>\n")
    string(APPEND HTML "</head>\n<body>\n")

    string(APPEND HTML "<h1>Nexus Build Report</h1>\n")

    string(APPEND HTML "<h2>Build Configuration</h2>\n")
    string(APPEND HTML "<table>\n")
    string(APPEND HTML "<tr><th>Property</th><th>Value</th></tr>\n")
    string(APPEND HTML "<tr><td>Platform</td><td>${CMAKE_SYSTEM_NAME}</td></tr>\n")
    string(APPEND HTML "<tr><td>Compiler</td><td>${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}</td></tr>\n")
    string(APPEND HTML "<tr><td>Build Type</td><td>${CMAKE_BUILD_TYPE}</td></tr>\n")
    string(APPEND HTML "<tr><td>CMake Version</td><td>${CMAKE_VERSION}</td></tr>\n")
    string(APPEND HTML "</table>\n")

    string(APPEND HTML "<h2>Build Statistics</h2>\n")
    string(APPEND HTML "<table>\n")
    string(APPEND HTML "<tr><th>Metric</th><th>Value</th></tr>\n")
    string(APPEND HTML "<tr><td>Start Time</td><td>${NEXUS_BUILD_START_TIME}</td></tr>\n")
    string(TIMESTAMP END_TIME "%Y-%m-%dT%H:%M:%S")
    string(APPEND HTML "<tr><td>End Time</td><td>${END_TIME}</td></tr>\n")
    string(APPEND HTML "<tr><td>Files Compiled</td><td>${NEXUS_BUILD_FILE_COUNT}</td></tr>\n")
    string(APPEND HTML "<tr><td>Errors</td><td>${NEXUS_BUILD_ERROR_COUNT}</td></tr>\n")
    string(APPEND HTML "<tr><td>Warnings</td><td>${NEXUS_BUILD_WARNING_COUNT}</td></tr>\n")
    string(APPEND HTML "</table>\n")

    string(APPEND HTML "<h2>Build Status</h2>\n")
    if(NEXUS_BUILD_ERROR_COUNT GREATER 0)
        string(APPEND HTML "<p class=\"failed\">FAILED</p>\n")
    else()
        string(APPEND HTML "<p class=\"success\">SUCCESS</p>\n")
    endif()

    string(APPEND HTML "</body>\n</html>\n")

    file(WRITE "${REPORT_FILE}" "${HTML}")

    message(STATUS "HTML build report generated: ${REPORT_FILE}")
endfunction()

#
# Generate JSON format build report
#
function(nexus_generate_json_report)
    # Use configured directory or default to CMAKE_BINARY_DIR
    if(DEFINED NEXUS_REPORT_DIR)
        set(REPORT_FILE "${NEXUS_REPORT_DIR}/nexus_build_report.json")
    else()
        set(REPORT_FILE "${CMAKE_BINARY_DIR}/nexus_build_report.json")
    endif()

    string(TIMESTAMP END_TIME "%Y-%m-%dT%H:%M:%S")

    set(JSON "{\n")
    string(APPEND JSON "  \"build_configuration\": {\n")
    string(APPEND JSON "    \"platform\": \"${CMAKE_SYSTEM_NAME}\",\n")
    string(APPEND JSON "    \"compiler\": \"${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}\",\n")
    string(APPEND JSON "    \"build_type\": \"${CMAKE_BUILD_TYPE}\",\n")
    string(APPEND JSON "    \"cmake_version\": \"${CMAKE_VERSION}\"\n")
    string(APPEND JSON "  },\n")
    string(APPEND JSON "  \"build_statistics\": {\n")
    string(APPEND JSON "    \"start_time\": \"${NEXUS_BUILD_START_TIME}\",\n")
    string(APPEND JSON "    \"end_time\": \"${END_TIME}\",\n")
    string(APPEND JSON "    \"files_compiled\": ${NEXUS_BUILD_FILE_COUNT},\n")
    string(APPEND JSON "    \"errors\": ${NEXUS_BUILD_ERROR_COUNT},\n")
    string(APPEND JSON "    \"warnings\": ${NEXUS_BUILD_WARNING_COUNT}\n")
    string(APPEND JSON "  },\n")

    if(NEXUS_BUILD_ERROR_COUNT GREATER 0)
        string(APPEND JSON "  \"build_status\": \"FAILED\"\n")
    else()
        string(APPEND JSON "  \"build_status\": \"SUCCESS\"\n")
    endif()

    string(APPEND JSON "}\n")

    file(WRITE "${REPORT_FILE}" "${JSON}")

    message(STATUS "JSON build report generated: ${REPORT_FILE}")
endfunction()

##############################################################################
# Increment File Count
##############################################################################

#
# Increment build file count
#
function(nexus_increment_file_count)
    math(EXPR FILE_COUNT "${NEXUS_BUILD_FILE_COUNT} + 1")
    set(NEXUS_BUILD_FILE_COUNT ${FILE_COUNT} CACHE INTERNAL "Build file count")
endfunction()

