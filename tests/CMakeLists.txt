# Unit tests (doctest), one executable per library; benches are separate
# programs declared to ctest. Labels: fast (every commit), full, local
# (needs data that is not in the repository), hw (needs the real machine).

add_executable(emucpc_test_toolchain toolchain_test.cpp)
target_link_libraries(emucpc_test_toolchain PRIVATE
    emucpc::settings emucpc::version emucpc::doctest emucpc::json)
add_test(NAME toolchain COMMAND emucpc_test_toolchain)
set_tests_properties(toolchain PROPERTIES LABELS fast)

add_test(NAME emucpc_version COMMAND emucpc --version)
set_tests_properties(emucpc_version PROPERTIES
    LABELS fast PASS_REGULAR_EXPRESSION "^EmuCPC [^\r\n]+\n$")

add_test(NAME emucpc_bad_option COMMAND emucpc --no-such-option)
set_tests_properties(emucpc_bad_option PROPERTIES LABELS fast WILL_FAIL TRUE)
