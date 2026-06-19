target_sources(objects PRIVATE
    uart.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_SOURCE_DIR}/kernel/include
)

add_subdirectory(gfx)
add_subdirectory(input)