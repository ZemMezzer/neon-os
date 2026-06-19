target_sources(objects PRIVATE
    diskio.c
    ff.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)