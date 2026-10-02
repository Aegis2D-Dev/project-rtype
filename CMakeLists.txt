cmake_minimum_required(VERSION 3.30)
project(rtype VERSION 0.1.0 LANGUAGES CXX)

option(RTYPE_BUILD_CLIENT "Build r-type_client (needs SDL3 and OpenGL)" ON)
set(AEGIS2D_BUILD_CLIENT ${RTYPE_BUILD_CLIENT} CACHE BOOL "Build platform, render and audio" FORCE)

include(FetchContent)
FetchContent_Declare(aegis2d
    GIT_REPOSITORY https://github.com/Aegis2D-Dev/aegis2d.git
    GIT_TAG master
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(aegis2d)

function(rtype_executable target)
    target_compile_features(${target} PRIVATE cxx_std_26)
    target_compile_definitions(${target} PRIVATE RTYPE_VERSION="${PROJECT_VERSION}")
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}>")
    aegis2d_stage_runtime(${target})
endfunction()

add_subdirectory(server)
if(RTYPE_BUILD_CLIENT)
    add_subdirectory(client)
endif()
