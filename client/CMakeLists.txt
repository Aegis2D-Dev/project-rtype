add_executable(r-type_client main.cpp)
target_link_libraries(r-type_client PRIVATE rtype_client_net aegis2d::engine aegis2d::warnings)
rtype_executable(r-type_client)
