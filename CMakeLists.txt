cmake_minimum_required(VERSION 3.20)

# The release version comes from the git tag: CI passes -DSNAPFORMAT_VERSION=X.Y.Z.
# Local builds keep 0.0.0 so they are never mistaken for a release.
set(SNAPFORMAT_VERSION "0.0.0" CACHE STRING "SnapFormat version (X.Y.Z), set by CI from the release tag")
if (NOT SNAPFORMAT_VERSION MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$")
  message(FATAL_ERROR "SNAPFORMAT_VERSION must look like X.Y.Z, got '${SNAPFORMAT_VERSION}'")
endif()

project(win11_quick_converter VERSION ${SNAPFORMAT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  libwebp
  GIT_REPOSITORY https://github.com/webmproject/libwebp.git
  GIT_TAG        v1.4.0
)
FetchContent_MakeAvailable(libwebp)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(CTest)

add_subdirectory(src/converter-cli)
add_subdirectory(src/shell-ext)

function(win11qc_set_target_output_directories target_name)
	set_target_properties(${target_name}
		PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
			LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
			ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
			PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
	)
endfunction()

win11qc_set_target_output_directories(converter-cli)
win11qc_set_target_output_directories(shell-ext)

# Version resource (Explorer > Properties > Details) for both binaries.
function(win11qc_add_version_resource target_name description original_filename)
	set(WIN11QC_FILE_DESCRIPTION "${description}")
	set(WIN11QC_ORIGINAL_FILENAME "${original_filename}")
	set(rc_file "${CMAKE_CURRENT_BINARY_DIR}/${target_name}-version.rc")
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in" "${rc_file}" @ONLY)
	target_sources(${target_name} PRIVATE "${rc_file}")
endfunction()

win11qc_add_version_resource(converter-cli "SnapFormat converter" "converter-cli.exe")
win11qc_add_version_resource(shell-ext "SnapFormat context menu" "shell-ext.dll")
