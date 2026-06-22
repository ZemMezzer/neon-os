target_sources(third_party PRIVATE
    lua_gfx.c
)

target_include_directories(third_party PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(bitmap)