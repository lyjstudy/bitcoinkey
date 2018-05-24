add_library(bitcoinconsensus
	base58.cpp
	cashaddr.cpp
	cashaddrenc.cpp
	chainparams.cpp
	config.cpp
	consensus/merkle.cpp
	coins.cpp
	compressor.cpp
	dstencode.cpp
	globals.cpp
	core_read.cpp
	core_write.cpp
	key.cpp
	keystore.cpp
	netaddress.cpp
	netbase.cpp
	primitives/block.cpp
	primitives/transaction.cpp
	protocol.cpp
	scheduler.cpp
	script/sign.cpp
	script/standard.cpp
	warnings.cpp
	arith_uint256.cpp
	hash.cpp
	pubkey.cpp
	script/bitcoinconsensus.cpp
	script/interpreter.cpp
	script/script.cpp
	script/script_error.cpp
	script/scriptcache.cpp
	script/sigcache.cpp
	script/ismine.cpp
	policy/fees.cpp
	policy/policy.cpp
)


target_link_libraries(bitcoinconsensus util secp256k1)
target_include_directories(bitcoinconsensus
    PRIVATE ${PROJECT_SOURCE_DIR}/core/libserver
	PUBLIC
		.
)