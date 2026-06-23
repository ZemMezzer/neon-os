target_compile_definitions(objects PRIVATE
    PACKAGES_ENABLED=1
)

target_compile_definitions(third_party PRIVATE
    PACKAGES_ENABLED=1
)


target_sources(objects PRIVATE
    package_manager.c
    package_runner.c
    package_shell.c
)

target_include_directories(third_party PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)