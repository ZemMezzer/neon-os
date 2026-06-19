target_sources(objects PRIVATE
    console.c
    shell.c
    shell_commands.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)