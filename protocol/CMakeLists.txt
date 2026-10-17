# Messages of the protocol shared by the server and the client (docs/rfc in the engine).
add_library(rtype_protocol STATIC
    events.cpp
    login.cpp
    snapshot.cpp)
target_include_directories(rtype_protocol PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(rtype_protocol
    PUBLIC rtype_game aegis2d::net
    PRIVATE aegis2d::warnings)
target_compile_features(rtype_protocol PUBLIC cxx_std_26)
set_target_properties(rtype_protocol PROPERTIES CXX_EXTENSIONS OFF)
