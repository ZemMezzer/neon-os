set(LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
    CACHE FILEPATH "Linker script for selected platform"
    FORCE
)

target_sources(objects PRIVATE
    uart.c
    Boot.S
)

if(GFX_ENABLED)
    add_subdirectory(gfx)
endif()

add_subdirectory(input)
add_subdirectory(fs)
