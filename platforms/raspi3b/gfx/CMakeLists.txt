target_sources(objects PRIVATE
    framebuffer.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
