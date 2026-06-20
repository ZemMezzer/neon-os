cmake_minimum_required(VERSION 3.20)

project(NeonOS C ASM)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)
set(PLATFORM "" CACHE STRING "Target platform")

option(LUA_ENABLED "Lua" ON)

add_library(objects OBJECT)
add_library(third_party OBJECT)

target_compile_options(objects PRIVATE
    -Wall
    -Wextra
    -O2
    -ffreestanding
    -fno-builtin
    -fno-tree-loop-distribute-patterns
    -nostdlib
    -nostartfiles
)

target_compile_options(third_party PRIVATE
    -Wall
    -Wextra
    -O0
    -fno-strict-aliasing
    -ffreestanding
    -fno-builtin
    -fno-tree-loop-distribute-patterns
    -nostdlib
    -nostartfiles
)

add_subdirectory(kernel)

if(PLATFORM STREQUAL "qemu")
    add_subdirectory(platforms/qemu)
else()
    message(FATAL_ERROR "Unknown PLATFORM: ${PLATFORM}")
endif()

if(LUA_ENABLED)
    target_compile_definitions(objects PRIVATE
        LUA_ENABLED=1
    )
    add_subdirectory(lua)
endif()

add_executable(${PROJECT_NAME}
    boot.S
    $<TARGET_OBJECTS:third_party>
    $<TARGET_OBJECTS:objects>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -O2
    -ffreestanding
    -fno-builtin
    -fno-tree-loop-distribute-patterns
    -nostdlib
    -nostartfiles
)

target_link_options(${PROJECT_NAME} PRIVATE
    -T${LINKER_SCRIPT}
    -ffreestanding
    -nostdlib
    -nostartfiles
    -Wl,--gc-sections
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "kernel.elf"
    LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kernel8.img
    COMMAND ${CMAKE_OBJCOPY}
            -O binary
            $<TARGET_FILE:${PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/kernel8.img
    DEPENDS ${PROJECT_NAME}
    COMMENT "Creating kernel8.img"
)

add_custom_target(kernel_img ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kernel8.img
)