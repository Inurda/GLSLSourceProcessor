cmake_minimum_required(VERSION 3.31)

project(GLSLSourceProcessor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON FORCE)

add_library(glsl_sp INTERFACE)

target_include_directories(glsl_sp INTERFACE include)

option(GLSL_SP_BUILD_EXAMPLE "" ON)

if (${GLSL_SP_BUILD_EXAMPLE})
    message(STATUS "Including the GLSL example")

    add_executable(glsl_sp_example main.cpp)

    target_link_libraries(glsl_sp_example PRIVATE glsl_sp)

    # Copies the files located in ${CONTENT_ROOT}/shaders to the build directory on build
    add_custom_target(stage_shaders ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:glsl_sp_example>/shaders
    )

    add_dependencies(glsl_sp_example stage_shaders)
endif()