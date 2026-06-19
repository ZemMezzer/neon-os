target_sources(objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(gfx)
add_subdirectory(console)
add_subdirectory(input)