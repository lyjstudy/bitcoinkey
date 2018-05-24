
# Bitcoin server facilities
add_library(server
	addrman.cpp
	addrdb.cpp
	bloom.cpp
	blockencodings.cpp
	chain.cpp
	checkpoints.cpp
	httprpc.cpp
	httpserver.cpp
	init.cpp
	dbwrapper.cpp
	merkleblock.cpp
	miner.cpp
	net.cpp
	net_processing.cpp
	noui.cpp
	pow.cpp
	rest.cpp
	rpc/abc.cpp
	rpc/blockchain.cpp
	rpc/mining.cpp
	rpc/misc.cpp
	rpc/net.cpp
	rpc/rawtransaction.cpp
	rpc/server.cpp
	rpc/client.cpp
	timedata.cpp
	torcontrol.cpp
	txdb.cpp
	txmempool.cpp
	ui_interface.cpp
	validation.cpp
	validationinterface.cpp
	versionbits.cpp
)

# This require libevent
find_package(Event REQUIRED)

target_link_libraries(server
	${EVENT_LIBRARY}
	bitcoinconsensus
	leveldb
	memenv
)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(server ${EVENT_PTHREAD_LIBRARY})
endif()
target_include_directories(server
	PUBLIC
		.
		# To access the config.
		${CMAKE_CURRENT_SOURCE_DIR}
)