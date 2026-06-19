target_sources(objects PRIVATE
    virtio_blk.c
    block.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)