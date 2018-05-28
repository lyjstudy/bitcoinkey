add_library(script
    machine.cpp
    machine_env.cpp
)

target_include_directories(script
	PUBLIC
		.
)

target_link_libraries(script crypto)
