# Target helpers.
#
# emucpc_library(<name> <sources>...)
#   Static library emucpc_<name>, alias emucpc::<name>, public headers in
#   libs/<name>/include. Used by the libraries outside the core.
#
# emucpc_core_library(<name> <sources>... [LINK <core targets>...])
#   Same for a core library (z80, media, chips, machine, expr), which may
#   link only to other core libraries: any other link is refused here, at
#   configuration time. ci/check_core.sh checks includes and symbols.

set(EMUCPC_CORE_LIBRARIES z80 media chips machine expr)

function(emucpc_library name)
    add_library(emucpc_${name} STATIC ${ARGN})
    add_library(emucpc::${name} ALIAS emucpc_${name})
    target_include_directories(emucpc_${name} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(emucpc_${name} PRIVATE emucpc::settings)
endfunction()

function(emucpc_core_library name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "LINK")
    if(NOT name IN_LIST EMUCPC_CORE_LIBRARIES)
        message(FATAL_ERROR "emucpc_core_library: '${name}' is not a core library")
    endif()
    foreach(dep IN LISTS arg_LINK)
        string(REGEX REPLACE "^emucpc(::|_)" "" short "${dep}")
        if(NOT short IN_LIST EMUCPC_CORE_LIBRARIES OR short STREQUAL dep)
            message(FATAL_ERROR
                "emucpc_core_library(${name}): link to '${dep}' refused, "
                "the core links only to core libraries")
        endif()
    endforeach()
    emucpc_library(${name} ${arg_UNPARSED_ARGUMENTS})
    if(arg_LINK)
        target_link_libraries(emucpc_${name} PUBLIC ${arg_LINK})
    endif()
endfunction()
