target_sources(objects PRIVATE
    sd.c
    block.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
