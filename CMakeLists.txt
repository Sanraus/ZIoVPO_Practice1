cmake_minimum_required(VERSION 3.21)

project(Practice1_CMakeBuild LANGUAGES CXX)

set(SOLUTION_FILE "${CMAKE_SOURCE_DIR}/Practice1.slnx")

if(NOT EXISTS "${SOLUTION_FILE}")
    message(FATAL_ERROR "Solution file not found: ${SOLUTION_FILE}")
endif()

find_program(MSBUILD_EXE MSBuild.exe)

if(NOT MSBUILD_EXE)
    message(FATAL_ERROR "MSBuild.exe not found. Open Developer PowerShell for Visual Studio or install Visual Studio Build Tools.")
endif()

add_custom_target(Practice1 ALL
    COMMAND "${MSBUILD_EXE}" "${SOLUTION_FILE}"
            /m
            /restore
            /p:Configuration=$<CONFIG>
            /p:Platform=x64
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building Practice1 WinUI 3 application through CMake"
    VERBATIM
)