target_sources(objects PRIVATE
    string.c
    memory.c
    stdio.c
    stdlib.c
    ctype.c
    format.c
    errno.c
    strtol.c
    strtod.c
    locale.c
    math.c
    time.c

    setjmp.S
)

target_include_directories(third_party PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)