target_sources(objects PRIVATE
    framebuffer.c
    fw_cfg.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)