target_sources(objects PRIVATE
    console.c
    shell.c
    shell_commands.c
    shell_alias.c
    shell_io.c
    shell_path.c
    shell_picker.c
    shell_script.c
    shell_util.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(third_party PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)