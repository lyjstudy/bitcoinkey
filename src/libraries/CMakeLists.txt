
add_subdirectory(leveldb)
target_include_directories(leveldb
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
           "${CMAKE_CURRENT_SOURCE_DIR}/leveldb/helpers/memenv"
)

add_subdirectory(secp256k1)
target_include_directories(secp256k1
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(univalue)
target_include_directories(univalue
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(crypto)
target_include_directories(crypto
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

# ZeroMQ
if(BUILD_BITCOIN_ZMQ)
	add_subdirectory(zmq)
    target_include_directories(zmq
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../core/bitcoinconsensus"
    )
endif()
