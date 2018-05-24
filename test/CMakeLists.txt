
set(abs_top_srcdir ${PROJECT_SOURCE_DIR})
set(abs_top_builddir ${PROJECT_BINARY_DIR})

configure_file (
    ${CMAKE_CURRENT_SOURCE_DIR}/config.ini.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.ini
)

# copy ?
configure_file (
    ${CMAKE_CURRENT_SOURCE_DIR}/functional/test_runner.py
    ${CMAKE_CURRENT_BINARY_DIR}/functional/test_runner.py
)