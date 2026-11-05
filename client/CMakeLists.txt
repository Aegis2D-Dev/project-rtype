# The graphical client: the window, the screens and the drawing of the game. Its logic that
# does not draw (session, world, connect form, notices) lives in client/network, and the
# connect screen's text fields in client/ui, where the tests reach them.
add_subdirectory(ui)
add_executable(r-type_client
    client_app.cpp
    game_art.cpp
    game_sounds.cpp
    game_view.cpp
    lagometer_view.cpp
    main.cpp
    menu_screens.cpp
    preferences.cpp)
target_link_libraries(r-type_client
    PRIVATE rtype_client_ui rtype_client_net aegis2d::engine aegis2d::warnings)
rtype_executable(r-type_client)
# The sprites and sounds, next to the executable (assets/sprites, assets/sounds).
aegis2d_copy_assets(r-type_client "${PROJECT_SOURCE_DIR}/assets")
