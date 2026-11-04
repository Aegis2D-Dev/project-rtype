# Game code shared by the server and the tests: tuning, collision layers, entities and the
# server simulation. Static, so the components it reads live in the same binary as the
# game's own code.
add_library(rtype_game STATIC
    entities.cpp
    position_history.cpp
    server_game.cpp
    waves.cpp)
target_include_directories(rtype_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(rtype_game
    PUBLIC aegis2d::core aegis2d::ecs aegis2d::shmup
    PRIVATE aegis2d::warnings)
target_compile_features(rtype_game PUBLIC cxx_std_26)
set_target_properties(rtype_game PROPERTIES CXX_EXTENSIONS OFF)
