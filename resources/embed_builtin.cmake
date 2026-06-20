if(NOT DEFINED INPUT_DIR OR
   NOT DEFINED OUTPUT_C OR
   NOT DEFINED OUTPUT_H)
    message(FATAL_ERROR "INPUT_DIR, OUTPUT_C and OUTPUT_H are required")
endif()

file(GLOB_RECURSE EMBED_FILES
    LIST_DIRECTORIES false
    "${INPUT_DIR}/*"
)

list(SORT EMBED_FILES)

file(WRITE "${OUTPUT_H}" [=[
#pragma once

#include <stddef.h>

typedef struct NeonBuiltinApp {
    const char* path;
    const unsigned char* data;
    size_t size;
} NeonBuiltinApp;

extern const NeonBuiltinApp neon_builtin_apps[];
extern const size_t neon_builtin_apps_count;
]=])

file(WRITE "${OUTPUT_C}" [=[
#include "builtin_apps_data.h"

]=])

set(TABLE_ENTRIES "")
set(FILE_INDEX 0)

foreach(FILE_PATH IN LISTS EMBED_FILES)
    file(RELATIVE_PATH RELATIVE_PATH "${INPUT_DIR}" "${FILE_PATH}")
    string(REPLACE "\\" "/" RELATIVE_PATH "${RELATIVE_PATH}")

    set(DISK_PATH "0:/${RELATIVE_PATH}")

    string(REPLACE "\\" "\\\\" C_PATH "${DISK_PATH}")
    string(REPLACE "\"" "\\\"" C_PATH "${C_PATH}")

    file(READ "${FILE_PATH}" FILE_HEX HEX)
    string(LENGTH "${FILE_HEX}" HEX_LENGTH)
    math(EXPR BYTE_COUNT "${HEX_LENGTH} / 2")

    set(ARRAY_NAME "neon_builtin_file_${FILE_INDEX}")
    file(APPEND "${OUTPUT_C}"
        "static const unsigned char ${ARRAY_NAME}[] = {\n"
    )

    if(BYTE_COUNT EQUAL 0)
        file(APPEND "${OUTPUT_C}" "    0x00\n")
    else()
        math(EXPR LAST_BYTE "${BYTE_COUNT} - 1")

        foreach(BYTE_INDEX RANGE 0 ${LAST_BYTE})
            math(EXPR HEX_INDEX "${BYTE_INDEX} * 2")
            string(SUBSTRING "${FILE_HEX}" ${HEX_INDEX} 2 BYTE_HEX)

            math(EXPR COLUMN "${BYTE_INDEX} % 12")
            if(COLUMN EQUAL 0)
                file(APPEND "${OUTPUT_C}" "    ")
            endif()

            file(APPEND "${OUTPUT_C}" "0x${BYTE_HEX}")

            if(NOT BYTE_INDEX EQUAL LAST_BYTE)
                file(APPEND "${OUTPUT_C}" ", ")
            endif()

            if(COLUMN EQUAL 11)
                file(APPEND "${OUTPUT_C}" "\n")
            endif()
        endforeach()

        math(EXPR LAST_COLUMN "${LAST_BYTE} % 12")
        if(NOT LAST_COLUMN EQUAL 11)
            file(APPEND "${OUTPUT_C}" "\n")
        endif()
    endif()

    file(APPEND "${OUTPUT_C}" "};\n\n")

    string(APPEND TABLE_ENTRIES
        "    { \"${C_PATH}\", ${ARRAY_NAME}, ${BYTE_COUNT} },\n"
    )

    math(EXPR FILE_INDEX "${FILE_INDEX} + 1")
endforeach()

if(FILE_INDEX EQUAL 0)
    file(APPEND "${OUTPUT_C}" [=[
static const unsigned char neon_builtin_empty_data[] = { 0x00 };

const NeonBuiltinApp neon_builtin_apps[] = {
    { 0, neon_builtin_empty_data, 0 }
};

const size_t neon_builtin_apps_count = 0;
]=])
else()
    file(APPEND "${OUTPUT_C}" "const NeonBuiltinApp neon_builtin_apps[] = {\n")
    file(APPEND "${OUTPUT_C}" "${TABLE_ENTRIES}")
    file(APPEND "${OUTPUT_C}" "};\n\n")
    file(APPEND "${OUTPUT_C}"
        "const size_t neon_builtin_apps_count = "
        "sizeof(neon_builtin_apps) / sizeof(neon_builtin_apps[0]);\n"
    )
endif()
