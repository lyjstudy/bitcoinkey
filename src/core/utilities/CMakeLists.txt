add_library(utilities
    arith_uint256.cpp
    uint256.cpp
    utilstrencodings.cpp
)

target_include_directories(utilities
	PUBLIC
		.
)

target_link_libraries(utilities crypto)

include(TestSuite)
#create_test_suite(bitcoin)
add_dependencies(check check-bitcoin)

add_test_to_suite(bitcoin utilities_tests
test/uint256_tests.cpp
test/test_main.cpp
)

find_package(Boost 1.58 REQUIRED unit_test_framework)

target_link_libraries(utilities_tests utilities crypto Boost::unit_test_framework)
