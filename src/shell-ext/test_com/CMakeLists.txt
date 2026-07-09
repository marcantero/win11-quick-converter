cmake_minimum_required(VERSION 3.15)
project(shell_ext_test_com LANGUAGES CXX)

add_executable(shell_ext_test_com main.cpp)

target_include_directories(shell_ext_test_com PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(shell_ext_test_com PRIVATE ole32)

set_target_properties(shell_ext_test_com PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

target_compile_features(shell_ext_test_com PRIVATE cxx_std_20)
