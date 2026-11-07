# Catch2 comes from the engine's CPM, as the engine only fetches it for its own tests.
CPMAddPackage(NAME Catch2 GITHUB_REPOSITORY catchorg/Catch2 VERSION 3.16.0 SYSTEM YES
    OPTIONS "CATCH_INSTALL_DOCS OFF" "CATCH_INSTALL_EXTRAS OFF")
target_compile_features(Catch2 PUBLIC cxx_std_17)
list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
include(Catch)

add_executable(rtype_tests
    bot_test.cpp
    chat_test.cpp
    client_session_test.cpp
    client_world_test.cpp
    compact_test.cpp
    connect_form_test.cpp
    delta_snapshot_test.cpp
    entities_test.cpp
    events_test.cpp
    game_instance_test.cpp
    input_test.cpp
    lag_compensation_test.cpp
    lagometer_test.cpp
    login_test.cpp
    network_stats_test.cpp
    player_notices_test.cpp
    position_history_test.cpp
    rate_limit_test.cpp
    room_test.cpp
    rtt_test.cpp
    server_game_test.cpp
    server_test.cpp
    ship_movement_test.cpp
    ship_prediction_test.cpp
    simulation_test.cpp
    snapshot_decoder_test.cpp
    snapshot_history_test.cpp
    snapshot_test.cpp
    stop_signal_test.cpp
    traffic_test.cpp
    waves_test.cpp)
target_link_libraries(rtype_tests PRIVATE rtype_game rtype_server rtype_client_net rtype_bot Catch2::Catch2WithMain
    aegis2d::warnings)
# The tests read the waves files of the repository, not the copies next to r-type_server.
target_compile_definitions(rtype_tests PRIVATE RTYPE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")
target_compile_features(rtype_tests PRIVATE cxx_std_26)
set_target_properties(rtype_tests PROPERTIES CXX_EXTENSIONS OFF)
# With the client: the connect screen's text fields, which need the engine's ui module.
if(TARGET rtype_client_ui)
    target_sources(rtype_tests PRIVATE connect_fields_test.cpp)
    target_link_libraries(rtype_tests PRIVATE rtype_client_ui)
endif()
aegis2d_stage_runtime(rtype_tests)

catch_discover_tests(rtype_tests DISCOVERY_MODE PRE_TEST)

# The report of tools/net_benchmark.py, when Python is there.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME net_benchmark_report
        COMMAND Python3::Interpreter -B -m unittest -v net_benchmark_test
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
