cmake_minimum_required(VERSION 3.20)

project(NeonOS C ASM)

set(PLATFORM "" CACHE STRING "Target platform")
set(ARCH "" CACHE STRING "Target arch")

option(LUA_ENABLED "Lua" ON)
option(GFX_ENABLED "GFX" ON)
option(BUILTIN_RESOURCES_ENABLED "RESOURCES" ON)
option(SHELL_STD "SHELL STD" ON)
option(PACKAGES_ENABLED "PACKAGES MANAGEMENT" ON)

add_library(objects OBJECT)
add_library(third_party OBJECT)

target_compile_options(objects PRIVATE
    -Wall
    -Wextra
    -O0
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

add_subdirectory(third_party)
add_subdirectory(arch)
add_subdirectory(kernel)
add_subdirectory(platforms)
add_subdirectory(bin)

if(LUA_ENABLED)
    add_subdirectory(lua)
endif()

if(BUILTIN_RESOURCES_ENABLED)
    add_subdirectory(resources)
endif()

add_executable(${PROJECT_NAME}
    $<TARGET_OBJECTS:third_party>
    $<TARGET_OBJECTS:objects>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -O0
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