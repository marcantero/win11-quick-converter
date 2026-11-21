add_executable(converter-cli
    src/main.cpp
    src/common.cpp
    src/document_converter.cpp
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
        advapi32
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

    # Stand-in for soffice.exe so the document path is tested without LibreOffice.
    # Built into the test directory so it never lands next to the shipped binaries.
    add_executable(fake-soffice tests/fake_soffice.cpp)
    target_compile_features(fake-soffice PRIVATE cxx_std_20)
    set_target_properties(fake-soffice PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-bin/$<CONFIG>")
    if (MSVC)
        target_compile_options(fake-soffice PRIVATE /W4 /permissive-)
    endif()
    if (MINGW)
        target_link_options(fake-soffice PRIVATE -municode)
    endif()

    add_test(
        NAME converter-cli.document_pdf
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/document_pdf_test.ps1
                -Cli $<TARGET_FILE:converter-cli>
                -FakeSoffice $<TARGET_FILE:fake-soffice>
                -WorkDir ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()
