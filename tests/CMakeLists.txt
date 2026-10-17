# Catch2 comes from the engine's CPM, as the engine only fetches it for its own tests.
CPMAddPackage(NAME Catch2 GITHUB_REPOSITORY catchorg/Catch2 VERSION 3.16.0 SYSTEM YES
    OPTIONS "CATCH_INSTALL_DOCS OFF" "CATCH_INSTALL_EXTRAS OFF")
target_compile_features(Catch2 PUBLIC cxx_std_17)
list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
include(Catch)

add_executable(rtype_tests
    client_session_test.cpp
    client_world_test.cpp
    entities_test.cpp
    events_test.cpp
    game_instance_test.cpp
    login_test.cpp
    rate_limit_test.cpp
    server_game_test.cpp
    server_test.cpp
    simulation_test.cpp
    snapshot_test.cpp
    stop_signal_test.cpp
    waves_test.cpp)
target_link_libraries(rtype_tests PRIVATE rtype_game rtype_server rtype_client_net Catch2::Catch2WithMain
    aegis2d::warnings)
# The tests read the waves files of the repository, not the copies next to r-type_server.
target_compile_definitions(rtype_tests PRIVATE RTYPE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")
target_compile_features(rtype_tests PRIVATE cxx_std_26)
set_target_properties(rtype_tests PROPERTIES CXX_EXTENSIONS OFF)
aegis2d_stage_runtime(rtype_tests)

catch_discover_tests(rtype_tests DISCOVERY_MODE PRE_TEST)
