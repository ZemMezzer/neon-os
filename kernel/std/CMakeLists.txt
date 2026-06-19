target_sources(objects PRIVATE
    string.c
    memory.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)