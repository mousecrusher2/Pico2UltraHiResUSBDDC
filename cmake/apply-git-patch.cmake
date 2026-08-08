if(NOT DEFINED GIT_EXECUTABLE)
    message(FATAL_ERROR "GIT_EXECUTABLE is required")
endif()
if(NOT DEFINED PATCH_SOURCE_DIR)
    message(FATAL_ERROR "PATCH_SOURCE_DIR is required")
endif()
if(NOT DEFINED PATCH_FILE)
    message(FATAL_ERROR "PATCH_FILE is required")
endif()

get_filename_component(PATCH_NAME "${PATCH_FILE}" NAME)

# FetchContent may run PATCH_COMMAND again when its declaration changes. Treat
# an already-applied patch as success, but reject partial or conflicting states.
execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${PATCH_FILE}"
    WORKING_DIRECTORY "${PATCH_SOURCE_DIR}"
    RESULT_VARIABLE PATCH_REVERSE_CHECK_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)
if(PATCH_REVERSE_CHECK_RESULT EQUAL 0)
    message(STATUS "Patch already applied: ${PATCH_NAME}")
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check "${PATCH_FILE}"
    WORKING_DIRECTORY "${PATCH_SOURCE_DIR}"
    RESULT_VARIABLE PATCH_CHECK_RESULT
    OUTPUT_VARIABLE PATCH_CHECK_OUTPUT
    ERROR_VARIABLE PATCH_CHECK_ERROR
)
if(NOT PATCH_CHECK_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Patch cannot be applied: ${PATCH_NAME}\n"
        "${PATCH_CHECK_OUTPUT}${PATCH_CHECK_ERROR}"
    )
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn "${PATCH_FILE}"
    WORKING_DIRECTORY "${PATCH_SOURCE_DIR}"
    RESULT_VARIABLE PATCH_APPLY_RESULT
    OUTPUT_VARIABLE PATCH_APPLY_OUTPUT
    ERROR_VARIABLE PATCH_APPLY_ERROR
)
if(NOT PATCH_APPLY_RESULT EQUAL 0)
    message(FATAL_ERROR
        "Failed to apply patch: ${PATCH_NAME}\n"
        "${PATCH_APPLY_OUTPUT}${PATCH_APPLY_ERROR}"
    )
endif()

message(STATUS "Applied patch: ${PATCH_NAME}")
