target_sources(objects PRIVATE
    input.c
    virtio_mmio.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel/input
    ${CMAKE_SOURCE_DIR}/kernel/include
)