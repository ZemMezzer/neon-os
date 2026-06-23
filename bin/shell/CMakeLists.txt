target_compile_definitions(objects PRIVATE
    SHELL_STD=1
)

target_compile_definitions(third_party PRIVATE
    SHELL_STD=1
)

target_sources(objects PRIVATE
    register.c
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(third_party PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(alias)
add_subdirectory(append)
add_subdirectory(cat)
add_subdirectory(cd)
add_subdirectory(echo)
add_subdirectory(help)
add_subdirectory(ls)
add_subdirectory(mkdir)
add_subdirectory(mv)
add_subdirectory(open)
add_subdirectory(path)
add_subdirectory(pwd)
add_subdirectory(rm)
add_subdirectory(sh)
add_subdirectory(write)