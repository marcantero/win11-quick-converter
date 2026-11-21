add_executable(converter-cli
    src/main.cpp
    src/wic_converter.cpp
)

target_include_directories(converter-cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(converter-cli PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(converter-cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(converter-cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(converter-cli
    PRIVATE
        windowscodecs
        ole32
        oleaut32
        uuid
        webp
)

# MinGW needs -municode to use wmain as the entry point (MSVC detects it).
if (MINGW)
    target_link_options(converter-cli PRIVATE -municode)
endif()

if (BUILD_TESTING)
    foreach(test_name unicode_path same_file webp_limits)
        add_test(
            NAME converter-cli.${test_name}
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                    -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_name}_test.ps1
                    -Cli $<TARGET_FILE:converter-cli>
                    -WorkDir ${CMAKE_CURRENT_BINARY_DIR}
        )
    endforeach()
endif()
