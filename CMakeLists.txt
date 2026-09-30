cmake_minimum_required(VERSION 3.30)
project(rtype LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(aegis2d
    GIT_REPOSITORY https://github.com/Aegis2D-Dev/aegis2d.git
    GIT_TAG master
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(aegis2d)

add_executable(rtype main.cpp)
target_link_libraries(rtype PRIVATE aegis2d::engine)
set_target_properties(rtype PROPERTIES CXX_EXTENSIONS OFF)
aegis2d_stage_runtime(rtype)
