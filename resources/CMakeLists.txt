set(NEON_BUILTIN_FILES_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/builtin"
)

set(NEON_BUILTIN_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

set(NEON_BUILTIN_DATA_C
    "${NEON_BUILTIN_GENERATED_DIR}/builtin_apps_data.c"
)

set(NEON_BUILTIN_DATA_H
    "${NEON_BUILTIN_GENERATED_DIR}/builtin_apps_data.h"
)

file(GLOB_RECURSE NEON_BUILTIN_SOURCE_FILES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    "${NEON_BUILTIN_FILES_DIR}/*"
)

add_custom_command(
    OUTPUT
        "${NEON_BUILTIN_DATA_C}"
        "${NEON_BUILTIN_DATA_H}"

    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${NEON_BUILTIN_GENERATED_DIR}"

    COMMAND "${CMAKE_COMMAND}"
        "-DINPUT_DIR=${NEON_BUILTIN_FILES_DIR}"
        "-DOUTPUT_C=${NEON_BUILTIN_DATA_C}"
        "-DOUTPUT_H=${NEON_BUILTIN_DATA_H}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_builtin.cmake"

    DEPENDS
        ${NEON_BUILTIN_SOURCE_FILES}
        "${CMAKE_CURRENT_SOURCE_DIR}/embed_builtin.cmake"

    COMMENT "Embedding factory files from resources/builtin"
    VERBATIM
)

set_source_files_properties(
    "${NEON_BUILTIN_DATA_C}"
    PROPERTIES GENERATED TRUE
)

add_custom_target(neon_builtin_files
    DEPENDS
        "${NEON_BUILTIN_DATA_C}"
        "${NEON_BUILTIN_DATA_H}"
)

add_dependencies(objects neon_builtin_files)

target_sources(objects PRIVATE
    "${NEON_BUILTIN_DATA_C}"
)

target_include_directories(objects PRIVATE
    "${NEON_BUILTIN_GENERATED_DIR}"
)
