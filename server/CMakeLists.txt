add_executable(r-type_server main.cpp)
target_link_libraries(r-type_server PRIVATE
    rtype_game aegis2d::core aegis2d::ecs aegis2d::net aegis2d::warnings)
rtype_executable(r-type_server)
