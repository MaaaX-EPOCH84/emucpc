# emucpc-ssts: the Z80 against SingleStepTests. The data stay out of the
# repository; their folder comes from EMUCPC_SSTS_DIR (cache variable,
# initialised from the environment variable of the same name, else the
# usual place). Without data the bench exits 77 and ctest skips the test.
add_executable(emucpc-ssts main.cpp)
target_link_libraries(emucpc-ssts PRIVATE emucpc::settings emucpc::z80 emucpc::json)

if(DEFINED ENV{EMUCPC_SSTS_DIR})
    set(ssts_default "$ENV{EMUCPC_SSTS_DIR}")
else()
    set(ssts_default "$ENV{HOME}/dev/emucpc-refs/tests/singlestep_z80/v1")
endif()
set(EMUCPC_SSTS_DIR "${ssts_default}" CACHE PATH "SingleStepTests Z80 data (v1/*.json)")

# fast: the first 10 cases of every file (16 040 cases), bus activity
# included, at every commit.
add_test(NAME z80_ssts_sample
    COMMAND emucpc-ssts --dir "${EMUCPC_SSTS_DIR}" --cases 10 --cycles --report 3)
# full: every case, with the bus activity, on demand (ctest -L full).
add_test(NAME z80_ssts_full
    COMMAND emucpc-ssts --dir "${EMUCPC_SSTS_DIR}" --cycles --report 3)
set_tests_properties(z80_ssts_sample PROPERTIES LABELS "fast;local" SKIP_RETURN_CODE 77)
set_tests_properties(z80_ssts_full PROPERTIES LABELS "full;local" SKIP_RETURN_CODE 77)
