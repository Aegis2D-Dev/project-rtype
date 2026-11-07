# The parts of the client's screens that can be tested without a window: the connect screen's
# text fields and the keys of the lobby's screens. Built with the client only, as it needs the
# engine's ui module.
add_library(rtype_client_ui STATIC
    connect_fields.cpp
    menu_keys.cpp)
target_include_directories(rtype_client_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(rtype_client_ui
    PUBLIC rtype_client_net aegis2d::engine
    PRIVATE aegis2d::warnings)
target_compile_features(rtype_client_ui PUBLIC cxx_std_26)
set_target_properties(rtype_client_ui PROPERTIES CXX_EXTENSIONS OFF)
