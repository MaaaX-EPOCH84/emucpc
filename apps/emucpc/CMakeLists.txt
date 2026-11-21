add_executable(emucpc main.cpp)
target_link_libraries(emucpc PRIVATE emucpc::settings emucpc::version)
