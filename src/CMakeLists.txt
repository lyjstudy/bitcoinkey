# Copyright (c) 2017 The Bitcoin developers

cmake_minimum_required(VERSION 3.5)
project(BitcoinABC)

set(CMAKE_CXX_STANDARD 11)

# Default visibility is hidden on all targets.
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

option(BUILD_BITCOIN_WALLET "Activate the wallet functionality" ON)
option(BUILD_BITCOIN_ZMQ "Activate the ZeroMQ functionalities" ON)
option(BUILD_BITCOIN_SEEDER "Build bitcoin-seeder" ON)
option(BUILD_BITCOIN_CLI "Build bitcoin-cli" ON)
option(BUILD_BITCOIN_TX "Build bitcoin-tx" ON)
option(BUILD_BITCOIN_QT "Build bitcoin-qt" ON)

# Ensure that WINDRES_PREPROC is enabled when using windres.
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	list(APPEND CMAKE_RC_FLAGS "-DWINDRES_PREPROC")
endif()

# Enable warning
include(AddCompilerFlags)

add_c_compiler_flag(-Wnested-externs -Wstrict-prototypes)
add_compiler_flag(
	-Wall
	-Wextra
	-Wformat
	-Wvla
	-Wformat-security
	-Wcast-align
)

option(EXTRA_WARNINGS "Enable extra warnings" OFF)
if(EXTRA_WARNINGS)
	add_compiler_flag(-Wshadow)
	add_cxx_compiler_flag(-Wsuggest-override)
else()
	add_compiler_flag(-Wno-unused-parameter)
endif()

# libtool style configure
add_subdirectory(config)

add_subdirectory(libraries)
add_subdirectory(core)

# Test suite.
# add_subdirectory(test)


# bitcoin-seeder
if(BUILD_BITCOIN_SEEDER)
	add_subdirectory(seeder)
endif()

# bitcoin-cli
if(BUILD_BITCOIN_CLI)
	add_executable(bitcoin-cli bitcoin-cli.cpp)
	if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
		target_sources(bitcoin-cli PRIVATE bitcoin-cli-res.rc)
	endif()

	target_include_directories(bitcoin-cli PRIVATE ${EVENT_INCLUDE_DIR})
	target_link_libraries(bitcoin-cli bitcoinconsensus server ${EVENT_LIBRARY})
endif()

# bitcoin-tx
if(BUILD_BITCOIN_TX)
	add_executable(bitcoin-tx bitcoin-tx.cpp)
	if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
		target_sources(bitcoin-tx PRIVATE bitcoin-tx-res.rc)
	endif()

	target_link_libraries(bitcoin-tx bitcoinconsensus)
endif()

# bitcoind
add_executable(bitcoind bitcoind.cpp)
target_link_libraries(bitcoind server)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_sources(bitcoind PRIVATE bitcoind-res.rc)
endif()
# Wallet
if(BUILD_BITCOIN_WALLET)
	add_subdirectory(wallet)
	target_link_libraries(bitcoind wallet)
endif()
# ZeroMQ
if(BUILD_BITCOIN_ZMQ)
	target_link_libraries(bitcoind zmq)
endif()

# Bitcoin-qt
if(BUILD_BITCOIN_QT)
	add_subdirectory(qt)
endif()
