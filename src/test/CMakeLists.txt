CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS system filesystem thread program_options unit_test_framework REQUIRED)

set(JSON_TEST_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/data/script_valid.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/base58_keys_valid.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/sig_canonical.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/sig_noncanonical.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/base58_encode_decode.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/base58_keys_invalid.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/script_invalid.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/tx_invalid.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/tx_valid.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/sighash.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/merkle_roots_sapling.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/merkle_serialization_sapling.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/merkle_witness_serialization_sapling.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/merkle_path_sapling.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/merkle_commitments_sapling.json
        ${CMAKE_CURRENT_SOURCE_DIR}/data/sapling_key_components.json
        )

function(GenerateHeaders)
    set(OutputFileList "")
    foreach(file IN LISTS ARGN)
        get_filename_component(VARNAME ${file} NAME_WE)
        set(outFile ${file}.h)
        set(runCmd ${CMAKE_SOURCE_DIR}/contrib/devtools/gen-json-headers.sh)
        add_custom_command(
                OUTPUT ${outFile}
                COMMAND ${CMAKE_COMMAND} -E echo "namespace json_tests{" > ${outFile}
                COMMAND ${CMAKE_COMMAND} -E echo "unsigned const char ${VARNAME}[] = {" >> ${outFile}
                COMMAND ${runCmd} ${file} ${outFile}
                COMMAND ${CMAKE_COMMAND} -E echo "};};" >> ${outFile}
                DEPENDS ${file}
                COMMENT "Generating ${file}.h"
                VERBATIM

        )
        list(APPEND OutputFileList ${outFile})
    endforeach()
    install(FILES ${OutputFileList} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/data)
    add_custom_target(
            genHeaders ALL
            DEPENDS ${OutputFileList}
            COMMENT "Processing files..."
    )
endfunction()

GenerateHeaders(${JSON_TEST_FILES})

set(BITCOIN_TEST_SUITE
        ${CMAKE_CURRENT_SOURCE_DIR}/test_c_note.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test_c_note.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/sapling_test_fixture.h
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/sapling_test_fixture.cpp
        ${CMAKE_SOURCE_DIR}/src/wallet/test/wallet_test_fixture.h
        ${CMAKE_SOURCE_DIR}/src/wallet/test/wallet_test_fixture.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/utiltest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/utiltest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/json_test_vectors.h
        )

set(BITCOIN_TESTS
        ${CMAKE_CURRENT_SOURCE_DIR}/arith_uint256_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/addrman_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/allocator_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/libsapling_utils_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/sapling_key_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/pedersen_hash_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/noteencryption_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/sapling_note_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/sapling_keystore_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/zip32_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/wallet_zkeys_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/merkletree_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/transaction_builder_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/sapling_wallet_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/base32_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/base58_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/base64_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bech32_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/budget_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bip32_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/checkblock_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Checkpoints_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/coins_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/convertbits_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/compress_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cuckoocache_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DoS_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/getarg_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/hash_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/key_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dbwrapper_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/main_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mempool_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/merkle_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/miner_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/multisig_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/net_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/netbase_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/pmt_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/policyestimator_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/prevector_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/random_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/reverselock_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rpc_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sanity_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/script_P2SH_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/script_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/script_standard_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/scriptnum_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/script_P2CS_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/serialize_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sighash_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sigopcount_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/skiplist_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sync_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/streams_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/timedata_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/torcontrol_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transaction_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/txvalidationcache_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/uint256_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/univalue_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/util_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/validation_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sha256compress_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/upgrades_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/validation_block_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/librust/sapling_rpc_wallet_tests.cpp
        ${CMAKE_SOURCE_DIR}/src/wallet/test/wallet_tests.cpp
        ${CMAKE_SOURCE_DIR}/src/wallet/test/crypto_tests.cpp
        ${CMAKE_SOURCE_DIR}/src/wallet/test/wallet_shielded_balances_tests.cpp
        ${CMAKE_SOURCE_DIR}/src/wallet/test/wallet_sapling_transactions_validations_tests.cpp
        )

set(test_test_c_note_SOURCES ${BITCOIN_TEST_SUITE} ${BITCOIN_TESTS} ${JSON_TEST_FILES})
add_executable(test_c_note ${test_test_c_note_SOURCES} ${BitcoinHeaders})
add_dependencies(test_c_note genHeaders libunivalue libsecp256k1 libzcashrust leveldb crc32c)
target_include_directories(test_c_note PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/leveldb
        ${CMAKE_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_SOURCE_DIR}/src/leveldb/helpers/memenv
        ${LIBEVENT_INCLUDE_DIR})
target_link_libraries(test_c_note PRIVATE
        SERVER_A
        CLI_A
        WALLET_A
        COMMON_A
        univalue
        UTIL_A
        SAPLING_A
        BITCOIN_CRYPTO_A
        leveldb
        crc32c
        secp256k1
        rustzcash
        ${BerkeleyDB_LIBRARIES} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} ${LIBEVENT_LIB} pthread
        )
if(GMP_FOUND)
    target_link_libraries(test_c_note PRIVATE ${GMP_LIBRARY})
    target_include_directories(test_c_note PRIVATE ${GMP_INCLUDE_DIR})
endif()
if(ZMQ_FOUND)
    target_link_libraries(test_c_note PRIVATE ZMQ_A ${ZMQ_LIB})
    target_include_directories(test_c_note PRIVATE ${ZMQ_INCLUDE_DIR})
endif()
if(MINIUPNP_FOUND)
    target_compile_definitions(test_c_note PRIVATE "-DSTATICLIB -DMINIUPNP_STATICLIB")
    target_link_libraries(test_c_note PRIVATE ${MINIUPNP_LIBRARY})
    target_include_directories(test_c_note PRIVATE ${MINIUPNP_INCLUDE_DIR})
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(test_c_note PRIVATE "-framework Cocoa")
endif()

target_link_libraries(test_c_note PRIVATE ${sodium_LIBRARY_RELEASE} -ldl -lpthread)

enable_testing()
add_test(NAME btest_c_note COMMAND test_c_note)
