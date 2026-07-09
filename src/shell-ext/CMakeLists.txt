add_library(shell-ext SHARED
    src/dummy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell-ext.def
)

target_include_directories(shell-ext
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(shell-ext PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(shell-ext PRIVATE /W4 /permissive-)
else()
    target_compile_options(shell-ext PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(shell-ext
    PRIVATE
        advapi32
        ole32
        shell32
        uuid
)

add_custom_command(TARGET shell-ext POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/snapformat.ico"
    $<TARGET_FILE_DIR:shell-ext>
)

add_subdirectory(test_com)
