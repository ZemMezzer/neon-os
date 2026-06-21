target_compile_definitions(objects PRIVATE
    ARCH_ARM64=1
)

if(ARCH STREQUAL "arm64")
    add_subdirectory(arm64)
else()
    message(FATAL_ERROR "Unknown ARCH: ${PLATFORM}")
endif()