target_sources(objects PRIVATE
    gfx.c
    font.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)