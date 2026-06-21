if(GFX_ENABLED)
    target_compile_definitions(objects PRIVATE
        GFX_ENABLED=1
    )

    target_compile_definitions(third_party PRIVATE
        GFX_ENABLED=1
    )
endif()

target_sources(objects PRIVATE
    gfx.c
    font.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(third_party PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)