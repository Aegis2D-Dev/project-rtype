# Server code shared by r-type_server and the tests: command line, sessions, sockets and
# the game instance.
add_library(rtype_server STATIC
    admin_console.cpp
    event_tracker.cpp
    game_instance.cpp
    game_server.cpp
    rate_limit.cpp
    room.cpp
    scoreboard.cpp
    server_network.cpp
    server_options.cpp
    snapshot_history.cpp
    sessions.cpp
    simulation.cpp
    stop_signal.cpp
    world_snapshot.cpp)
target_include_directories(rtype_server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
find_package(Threads REQUIRED)
target_link_libraries(rtype_server
    PUBLIC rtype_game rtype_protocol aegis2d::core aegis2d::net Threads::Threads
    PRIVATE aegis2d::warnings)
target_compile_features(rtype_server PUBLIC cxx_std_26)
set_target_properties(rtype_server PROPERTIES CXX_EXTENSIONS OFF)

add_executable(r-type_server main.cpp)
target_link_libraries(r-type_server PRIVATE
    rtype_server rtype_game aegis2d::core aegis2d::ecs aegis2d::net aegis2d::warnings)
rtype_executable(r-type_server)
# The waves files and other server data, copied next to r-type_server (core::asset_path()).
aegis2d_copy_assets(r-type_server "${PROJECT_SOURCE_DIR}/assets")
