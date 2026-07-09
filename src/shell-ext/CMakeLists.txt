add_library(shell-ext SHARED
    src/dummy.cpp
)

target_compile_features(shell-ext PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(shell-ext PRIVATE /W4 /permissive-)
else()
    target_compile_options(shell-ext PRIVATE -Wall -Wextra -Wpedantic)
endif()
