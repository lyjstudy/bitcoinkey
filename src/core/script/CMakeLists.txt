add_library(script
    machine.cpp
    machine_env.cpp
    script_error.cpp
)

target_include_directories(script
	PUBLIC
		.
)

target_link_libraries(script crypto)


include(TestSuite)
create_test_suite(bitcoin)
add_dependencies(check check-bitcoin)

add_test_to_suite(bitcoin script_tests
test/scriptnum_tests.cpp
test/machine_tests.cpp
test/test_main.cpp
)

find_package(Boost 1.58 REQUIRED unit_test_framework)

target_link_libraries(script_tests script Boost::unit_test_framework)
