cmake_minimum_required(VERSION 3.12)

project(3dscan)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MPP REQUIRED IMPORTED_TARGET
    rockchip_mpp
    rockchip_vpu
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -fcoroutines)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        add_compile_options(-mcpu=cortex-a76.cortex-a55)
    else()
        add_compile_options(-march=native)
    endif()
endif()

file(GLOB SOURCES
    "${CMAKE_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_SOURCE_DIR}/pipelines/stereo_pipe.cpp"
)

add_executable(stereo_pipe ${SOURCES})

set(STEREO_PIPE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(stereo_pipe BEFORE PRIVATE ${STEREO_PIPE_INCLUDES})

target_link_libraries(stereo_pipe PRIVATE
    ${OpenCV_LIBS}
    PkgConfig::MPP
)


