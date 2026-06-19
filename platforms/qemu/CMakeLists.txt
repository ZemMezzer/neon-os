target_sources(objects PRIVATE
    uart.c
)

add_subdirectory(gfx)
add_subdirectory(input)
add_subdirectory(fs)