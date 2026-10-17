cmake_minimum_required(VERSION 3.30)
project(rtype VERSION 0.2.0 LANGUAGES CXX)

option(RTYPE_BUILD_CLIENT "Build r-type_client (needs SDL3 and OpenGL)" ON)
option(RTYPE_BUILD_TESTS "Build the game unit tests" ${PROJECT_IS_TOP_LEVEL})
set(AEGIS2D_BUILD_CLIENT ${RTYPE_BUILD_CLIENT} CACHE BOOL "Build platform, render and audio" FORCE)

# Development builds follow the engine's integration branch, so the game gets each engine
# feature as soon as it is merged. FetchContent never updates a branch it has already
# checked out, and CI keeps its build trees between jobs, so the branch is resolved to its
# latest commit at each configure: a new commit changes GIT_TAG and is fetched. If git cannot
# reach the engine, the commit resolved last time is kept (offline configures still work),
# or the branch itself on a new build tree. The Epitech mirror skips all this: it bundles the
# engine's master through FETCHCONTENT_SOURCE_DIR_AEGIS2D.
set(RTYPE_ENGINE_REPOSITORY https://github.com/Aegis2D-Dev/aegis2d.git)
set(RTYPE_ENGINE_BRANCH develop CACHE STRING "Engine branch followed by development builds")
include(FetchContent)
set(rtype_engine_tag ${RTYPE_ENGINE_BRANCH})
set(rtype_engine_shallow TRUE)
if(NOT FETCHCONTENT_SOURCE_DIR_AEGIS2D AND NOT FETCHCONTENT_FULLY_DISCONNECTED)
    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env GIT_TERMINAL_PROMPT=0 ${GIT_EXECUTABLE} ls-remote
                    ${RTYPE_ENGINE_REPOSITORY} refs/heads/${RTYPE_ENGINE_BRANCH}
            OUTPUT_VARIABLE rtype_engine_head
            ERROR_QUIET
            TIMEOUT 60)
        string(REGEX MATCH "^[0-9a-f]+" rtype_engine_commit "${rtype_engine_head}")
    endif()
    if(rtype_engine_commit)
        set(RTYPE_ENGINE_COMMIT ${rtype_engine_commit} CACHE INTERNAL "Engine commit resolved last")
        message(STATUS "Engine: ${RTYPE_ENGINE_BRANCH} is at ${rtype_engine_commit}")
    else()
        message(WARNING "Engine: could not resolve ${RTYPE_ENGINE_BRANCH}; keeping the engine "
                        "of the last configure")
    endif()
    if(RTYPE_ENGINE_COMMIT)
        set(rtype_engine_tag ${RTYPE_ENGINE_COMMIT})
        set(rtype_engine_shallow FALSE)
    endif()
endif()
FetchContent_Declare(aegis2d
    GIT_REPOSITORY ${RTYPE_ENGINE_REPOSITORY}
    GIT_TAG ${rtype_engine_tag}
    GIT_SHALLOW ${rtype_engine_shallow})
FetchContent_MakeAvailable(aegis2d)

function(rtype_executable target)
    target_compile_features(${target} PRIVATE cxx_std_26)
    target_compile_definitions(${target} PRIVATE RTYPE_VERSION="${PROJECT_VERSION}")
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}>")
    aegis2d_stage_runtime(${target})
endfunction()

add_subdirectory(game)
add_subdirectory(protocol)
add_subdirectory(server)
add_subdirectory(client/network)
if(RTYPE_BUILD_CLIENT)
    add_subdirectory(client)
endif()

# What `cpack` puts in r-type-<version>-<system>-<cpu>.tar.gz (.zip on Windows): bin/ with the
# executables and their assets, and the engine libraries they need (lib/, or bin/ on Windows).
set(rtype_installed r-type_server)
if(RTYPE_BUILD_CLIENT)
    list(APPEND rtype_installed r-type_client)
endif()
aegis2d_install(TARGETS ${rtype_installed} ASSETS "${PROJECT_SOURCE_DIR}/assets")
# The engine's master sets the run path to the libraries in its own folder's scope, which a
# game fetched with FetchContent does not see: set it here, or the packaged binaries do not
# find the engine libraries.
if(APPLE)
    set_target_properties(${rtype_installed} PROPERTIES
        INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
elseif(NOT WIN32)
    set_target_properties(${rtype_installed} PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()
if(PROJECT_IS_TOP_LEVEL)
    aegis2d_package(NAME r-type VENDOR Aegis2D-Dev
        DESCRIPTION "R-Type: the authoritative server and the client of a four-player shoot'em up")
endif()

if(RTYPE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
