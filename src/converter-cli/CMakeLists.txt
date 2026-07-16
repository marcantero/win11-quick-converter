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
