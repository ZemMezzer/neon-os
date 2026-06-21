target_sources(objects PRIVATE
    uart.c
)

if(GFX_ENABLED)
    add_subdirectory(gfx)
endif()

add_subdirectory(input)
add_subdirectory(fs)