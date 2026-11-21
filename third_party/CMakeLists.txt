# Copies of small third-party libraries, each recorded in VERSIONS.md.
# Their headers are included as system headers: their warnings do not stop
# the build of our code (cmake/EmucpcWarnings.cmake).

add_library(emucpc_doctest INTERFACE)
add_library(emucpc::doctest ALIAS emucpc_doctest)
target_include_directories(emucpc_doctest SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/doctest")

add_library(emucpc_json INTERFACE)
add_library(emucpc::json ALIAS emucpc_json)
target_include_directories(emucpc_json SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json")
