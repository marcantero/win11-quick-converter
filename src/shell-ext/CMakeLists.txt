add_library(shell-ext SHARED
    src/dummy.cpp
    src/batch_report.cpp
    src/toast.cpp
    src/file_types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shell-ext.def
    ${CMAKE_CURRENT_SOURCE_DIR}/shell-ext.rc
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
        runtimeobject
        shell32
        shlwapi
        uuid
)

add_custom_command(TARGET shell-ext POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/snapformat.ico"
    $<TARGET_FILE_DIR:shell-ext>
)

add_subdirectory(test_com)

if (BUILD_TESTING)
    add_executable(shell-ext-batch-test tests/batch_unload_test.cpp)
    target_include_directories(shell-ext-batch-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(shell-ext-batch-test PRIVATE cxx_std_20)
    target_link_libraries(shell-ext-batch-test PRIVATE ole32 shell32)
    # The batch launches converter-cli.exe from the DLL's directory.
    add_dependencies(shell-ext-batch-test shell-ext converter-cli)

    add_test(
        NAME shell-ext.batch_unload
        COMMAND shell-ext-batch-test $<TARGET_FILE:shell-ext> ${CMAKE_CURRENT_BINARY_DIR}
    )
    # Don't pop real notifications on the developer's desktop during tests.
    set_tests_properties(shell-ext.batch_unload PROPERTIES ENVIRONMENT "SNAPFORMAT_NO_TOASTS=1")

    add_executable(shell-ext-report-test tests/batch_report_test.cpp src/batch_report.cpp)
    target_include_directories(shell-ext-report-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(shell-ext-report-test PRIVATE cxx_std_20)
    target_link_libraries(shell-ext-report-test PRIVATE shlwapi)
    add_test(NAME shell-ext.batch_report COMMAND shell-ext-report-test)

    add_executable(shell-ext-menu-test tests/menu_state_test.cpp src/file_types.cpp)
    target_include_directories(shell-ext-menu-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(shell-ext-menu-test PRIVATE cxx_std_20)
    target_link_libraries(shell-ext-menu-test PRIVATE ole32 shell32)
    add_dependencies(shell-ext-menu-test shell-ext)
    add_test(
        NAME shell-ext.menu_state
        COMMAND shell-ext-menu-test $<TARGET_FILE:shell-ext> ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()
