# Network side of the client: its session with the server. Built without the client's
# window and sound, so the server preset builds and tests it too.
add_library(rtype_client_net STATIC
    client_options.cpp
    client_session.cpp)
target_include_directories(rtype_client_net PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(rtype_client_net
    PUBLIC rtype_protocol aegis2d::core aegis2d::net
    PRIVATE aegis2d::warnings)
target_compile_features(rtype_client_net PUBLIC cxx_std_26)
set_target_properties(rtype_client_net PROPERTIES CXX_EXTENSIONS OFF)
