# r-type_bot: a player without a window, for load and network tests. Built on the client's
# network side only, so every preset builds it, the server one included.
add_library(rtype_bot STATIC
    bot.cpp
    bot_options.cpp
    input_script.cpp)
target_include_directories(rtype_bot PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(rtype_bot PUBLIC rtype_client_net PRIVATE aegis2d::warnings)
target_compile_features(rtype_bot PUBLIC cxx_std_26)
set_target_properties(rtype_bot PROPERTIES CXX_EXTENSIONS OFF)

add_executable(r-type_bot main.cpp)
target_link_libraries(r-type_bot PRIVATE rtype_bot aegis2d::core aegis2d::net aegis2d::warnings)
rtype_executable(r-type_bot)
