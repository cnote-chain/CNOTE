cmake_minimum_required(VERSION 3.10)

# Search for and use ccache if available to speed up build times
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    # Set up wrapper scripts
    set(C_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CXX_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Search for the cargo and rustc compilers required to build libzcashrust
find_program(CARGO_PROGRAM cargo REQUIRED)
if(CARGO_PROGRAM)
    message(STATUS "Found cargo: ${CARGO_PROGRAM}")
endif()

find_program(RUSTC_PROGRAM rustc REQUIRED)
if(RUSTC_PROGRAM)
    message(STATUS "Found rustc: ${RUSTC_PROGRAM}")
endif()

project(C_Note)

set(BDB_VER "4.8.30")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/contrib/cmake")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 ")
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    message(FATAL_ERROR "Native Windows CMake is not supported yet, use WSL instead")
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    message(STATUS "Compiling on macOS")
    set(ENV{target} "Mac")
    add_definitions("-DMAC_OSX")
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/qt5)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/openssl@1.1)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/Cellar/berkeley-db@4)
    #set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@1.1/")
    #set(ENV{CPPFLAGS} "-I${OPENSSL_ROOT_DIR}/include")
    #set(ENV{LDFLAGS} "-L${OPENSSL_ROOT_DIR}/lib")
    set(BerkeleyDB_ROOT_DIR "/usr/local/Cellar/berkeley-db@4/${BDB_VER}/")
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_NO_BOOST_CMAKE ON)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(ENV{target} "Linux")
    file(READ "/proc/version" _SYS_VERSION)
    string(REGEX MATCH "Microsoft" _SYSTEM_VERSION_PARSED "${_SYS_VERSION}")
    if(${_SYSTEM_VERSION_PARSED} MATCHES "Microsoft")
        message(STATUS "Compiling on WSL")
        set(ENV{DISPLAY} ":0")
        set(ENV{LIBGL_ALWAYS_INDIRECT} 1)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY /home/$ENV{USER}/c_note-run)
        message(WARNING "WSL Runtime support requires VcXsrv to be installed and running")
    else()
        message(STATUS "Compiling on Linux")
    endif()
    list(APPEND CMAKE_PREFIX_PATH /usr/lib/x86_64-linux-gnu/cmake/Qt5)
    set(Qt5core_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5/QtCore")
    find_path(ENDIAN_INCLUDES endian.h)
    if(ENDIAN_INCLUDES)
        message(STATUS "Found endian.h: ${ENDIAN_INCLUDES}")
    endif()
    #add_definitions("-D__BYTE_ORDER -D__LITTLE_ENDIAN -D__GLIBC__ -DHAVE_DECL_BSWAP_16=0 -DHAVE_DECL_BSWAP_32=0 -DHAVE_DECL_BSWAP_64=0")
endif()

# Find Dependencies
find_package(BerkeleyDB ${BDB_VER} REQUIRED)
if(BerkeleyDB_FOUND)
    if(NOT ${BerkeleyDB_VERSION} MATCHES "${BDB_VER}")
        message(WARNING "BerkeleyDB version other than ${BDB_VER} found!")
        set(BDB_CONFIGURE_FLAGS "--with-incompatible-bdb")
    endif()
endif()

find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL (${OPENSSL_VERSION}): ${OPENSSL_LIBRARIES}")
    if(OPENSSL_VERSION VERSION_GREATER_EQUAL 1.1)
        message(STATUS "Found unsupported OpenSSL version!")
        set(SSL_CONFIGURE_FLAGS "--with-unsupported-ssl")
    endif()
endif()

find_package(LibEvent REQUIRED)

find_package(GMP)
if(GMP_FOUND)
else()
    message(WARNING "GMP not found, falling back to OpenSSL for bignum!")
endif()

find_package(ZMQ)
find_package(Miniupnp)
find_package(Boost COMPONENTS system filesystem thread program_options REQUIRED)
find_package(Sodium REQUIRED)

# run autogen.sh if missing header files from configure on Linux/Mac
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/configure")
else()
    execute_process(
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/autogen.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# run configure if c_note_config.h doesn't exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/config/c_note-config.h")
else()
    execute_process(
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/configure ${CONFIGSITE} ${BDB_CONFIGURE_FLAGS} ${BIGNUM_CONFIGURE_FLAGS} ${SSL_CONFIGURE_FLAGS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# error out if configure fails
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/config/c_note-config.h")
else()
    execute_process(COMMAND cat config.log WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    message(FATAL_ERROR "configure script didn't finish properly see config.log output above")

endif()
add_definitions(-DHAVE_CONFIG_H)

ExternalProject_Add (
        libunivalue
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/univalue
        CONFIGURE_COMMAND ""
        BUILD_COMMAND $(MAKE)
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        )

ExternalProject_Add (
        libzcashrust
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${CARGO_PROGRAM} build --release
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        )

ExternalProject_Add (
        libsecp256k1
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/secp256k1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND $(MAKE)
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        )

set(CRC32C_BUILD_TESTS OFF CACHE BOOL "Build CRC32C's unit tests" FORCE)
set(CRC32C_BUILD_BENCHMARKS OFF CACHE BOOL "Build CRC32C's benchmarks" FORCE)
set(CRC32C_USE_GLOG OFF CACHE BOOL "Build CRC32C's tests with Google Logging" FORCE)
set(CRC32C_INSTALL OFF CACHE BOOL "Install CRC32C's header and library" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/crc32c)

set(LEVELDB_BUILD_TESTS OFF CACHE BOOL "Build LevelDB's unit tests" FORCE)
set(LEVELDB_BUILD_BENCHMARKS OFF CACHE BOOL "Build LevelDB's benchmarks" FORCE)
set(LEVELDB_INSTALL OFF CACHE BOOL "Install LevelDB's header and library" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/leveldb)

file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
file(GLOB CRYPTO_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/*.h)
file(GLOB PRIMITIVE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/primitives/*.h)
file(GLOB ZMQ_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/zmq/*.h)
file(GLOB SCRIPT_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/script/*.h)
file(GLOB RPC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/*.h)
file(GLOB COMPAT_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/compat/*.h)
file(GLOB CONSENSUS_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/consensus/*.h)
file(GLOB CTAES_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/ctaes/*.h)
file(GLOB ZRUST_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/rust/include/*.h)
file(GLOB SAPLING_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/sapling/*.h)

source_group("BitcoinHeaders" FILES
        ${HEADERS}
        ${CRYPTO_HEADERS}
        ${PRIMITIVE_HEADERS}
        ${ZMQ_HEADERS}
        ${SCRIPT_HEADERS}
        ${RPC_HEADERS}
        ${COMPAT_HEADERS}
        ${CONSENSUS_HEADERS}
        ${CTAES_HEADERS}
        ${ZRUST_HEADERS}
        ${SAPLING_HEADERS}
        ./src/support/cleanse.h
        )

set(SERVER_SOURCES
        ./src/addrdb.cpp
        ./src/addrman.cpp
        ./src/bloom.cpp
        ./src/blocksignature.cpp
        ./src/chain.cpp
        ./src/checkpoints.cpp
        ./src/httprpc.cpp
        ./src/httpserver.cpp
        ./src/indirectmap.h
        ./src/init.cpp
        ./src/interfaces/handler.cpp
        ./src/interfaces/wallet.cpp
        ./src/dbwrapper.cpp
        ./src/merkleblock.cpp
        ./src/miner.cpp
        ./src/blockassembler.cpp
        ./src/net.cpp
        ./src/net_processing.cpp
        ./src/noui.cpp
        ./src/policy/fees.cpp
        ./src/policy/policy.cpp
        ./src/pow.cpp
        ./src/rest.cpp
        ./src/rpc/blockchain.cpp
        ./src/rpc/masternode.cpp
        ./src/rpc/budget.cpp
        ./src/rpc/mining.cpp
        ./src/rpc/misc.cpp
        ./src/rpc/net.cpp
        ./src/rpc/rawtransaction.cpp
        ./src/rpc/server.cpp
        ./src/script/sigcache.cpp
        ./src/script/ismine.cpp
        ./src/sporkdb.cpp
        ./src/timedata.cpp
        ./src/torcontrol.cpp
        ./src/sapling/sapling_txdb.cpp
        ./src/txdb.cpp
        ./src/txmempool.cpp
        ./src/sapling/sapling_validation.cpp
        ./src/validation.cpp
        ./src/validationinterface.cpp
        )
add_library(SERVER_A STATIC ${BitcoinHeaders} ${SERVER_SOURCES})
if(MINIUPNP_FOUND)
    target_compile_definitions(SERVER_A PUBLIC "-DSTATICLIB -DMINIUPNP_STATICLIB")
endif()
target_include_directories(SERVER_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rust/include
    ${ZMQ_INCLUDE_DIR} ${LIBEVENT_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${BerkeleyDB_INCLUDE_DIRS}
    )

if(ZMQ_FOUND)
    set(ZMQ_SOURCES
        ./src/zmq/zmqabstractnotifier.cpp
        ./src/zmq/zmqnotificationinterface.cpp
        ./src/zmq/zmqpublishnotifier.cpp
    )
    add_library(ZMQ_A STATIC ${BitcoinHeaders} ${ZMQ_SOURCES} ${ZMQ_LIB})
    target_include_directories(ZMQ_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${ZMQ_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
    target_compile_definitions(ZMQ_A PUBLIC "-DZMQ_STATIC")
endif()

set(WALLET_SOURCES
        ./src/activemasternode.cpp
        ./src/bip38.cpp
        ./src/wallet/db.cpp
        ./src/addressbook.cpp
        ./src/crypter.cpp
        ./src/budget/budgetdb.cpp
        ./src/budget/budgetmanager.cpp
        ./src/budget/budgetproposal.cpp
        ./src/budget/budgetvote.cpp
        ./src/budget/finalizedbudget.cpp
        ./src/budget/finalizedbudgetvote.cpp
        ./src/masternode.cpp
        ./src/masternode-payments.cpp
        ./src/masternode-sync.cpp
        ./src/tiertwo_networksync.cpp
        ./src/masternodeconfig.cpp
        ./src/masternodeman.cpp
        ./src/messagesigner.cpp
        ./src/wallet/hdchain.cpp
        ./src/wallet/rpcdump.cpp
        ./src/wallet/scriptpubkeyman.cpp
        ./src/wallet/rpcwallet.cpp
        ./src/kernel.cpp
        ./src/legacy/stakemodifier.cpp
        ./src/wallet/wallet.cpp
        ./src/wallet/walletdb.cpp
        ./src/stakeinput.cpp
        )
add_library(WALLET_A STATIC ${BitcoinHeaders} ${WALLET_SOURCES})
target_include_directories(WALLET_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/secp256k1/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
        ${OPENSSL_INCLUDE_DIR}
        ${BerkeleyDB_INCLUDE_DIRS}
        )

set(BITCOIN_CRYPTO_SOURCES
        ./src/crypto/aes.cpp
        ./src/crypto/sha1.cpp
        ./src/crypto/sha256.cpp
        ./src/crypto/sha512.cpp
        ./src/crypto/chacha20.cpp
        ./src/crypto/hmac_sha256.cpp
        ./src/crypto/rfc6979_hmac_sha256.cpp
        ./src/crypto/hmac_sha512.cpp
        ./src/crypto/scrypt.cpp
        ./src/crypto/ripemd160.cpp
        ./src/crypto/aes_helper.c
        ./src/crypto/blake.c
        ./src/crypto/bmw.c
        ./src/crypto/groestl.c
        ./src/crypto/jh.c
        ./src/crypto/keccak.c
        ./src/crypto/skein.c
        ./src/crypto/common.h
        ./src/crypto/sha256.h
        ./src/crypto/sha512.h
        ./src/crypto/chacha20.h
        ./src/crypto/hmac_sha256.h
        ./src/crypto/rfc6979_hmac_sha256.h
        ./src/crypto/hmac_sha512.h
        ./src/crypto/scrypt.h
        ./src/crypto/sha1.h
        ./src/crypto/ripemd160.h
        ./src/crypto/sph_blake.h
        ./src/crypto/sph_bmw.h
        ./src/crypto/sph_groestl.h
        ./src/crypto/sph_jh.h
        ./src/crypto/sph_keccak.h
        ./src/crypto/sph_skein.h
        ./src/crypto/sph_types.h
        )
add_library(BITCOIN_CRYPTO_A STATIC ${BITCOIN_CRYPTO_SOURCES})
target_include_directories(BITCOIN_CRYPTO_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${OPENSSL_INCLUDE_DIR})

set(COMMON_SOURCES
        ./src/allocators.cpp
        ./src/base58.cpp
        ./src/bip38.cpp
        ./src/consensus/params.cpp
        ./src/consensus/upgrades.cpp
        ./src/chainparams.cpp
        ./src/coins.cpp
        ./src/key_io.cpp
        ./src/compressor.cpp
        ./src/tiertwo/specialtx_validation.cpp
        ./src/consensus/merkle.cpp
        ./src/consensus/tx_verify.cpp
        ./src/primitives/block.cpp
        ./src/primitives/transaction.cpp
        ./src/core_read.cpp
        ./src/core_write.cpp
        ./src/hash.cpp
        ./src/key.cpp
        ./src/keystore.cpp
        ./src/netaddress.cpp
        ./src/netbase.cpp
        ./src/policy/feerate.cpp
        ./src/protocol.cpp
        ./src/pubkey.cpp
        ./src/scheduler.cpp
        ./src/script/interpreter.cpp
        ./src/script/script.cpp
        ./src/script/sign.cpp
        ./src/script/standard.cpp
        ./src/script/script_error.cpp
        ./src/spork.cpp
        ./src/sporkdb.cpp
        ./src/warnings.cpp
        )
add_library(COMMON_A STATIC ${BitcoinHeaders} ${COMMON_SOURCES})
target_include_directories(COMMON_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/secp256k1/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
        ${OPENSSL_INCLUDE_DIR}
        ${BerkeleyDB_INCLUDE_DIRS}
        )

set(UTIL_SOURCES
        ./src/allocators.cpp
        ./src/compat/strnlen.cpp
        ./src/compat/glibc_sanity.cpp
        ./src/compat/glibcxx_sanity.cpp
        ./src/chainparamsbase.cpp
        ./src/clientversion.cpp
        ./src/fs.cpp
        ./src/logging.cpp
        ./src/random.cpp
        ./src/rpc/protocol.cpp
        ./src/sync.cpp
        ./src/threadinterrupt.cpp
        ./src/blob_uint256.cpp
        ./src/arith_uint256.cpp
        ./src/uint256.cpp
        ./src/util/threadnames.cpp
        ./src/util.cpp
        ./src/utilstrencodings.cpp
        ./src/utilmoneystr.cpp
        ./src/utiltime.cpp
        ./src/support/cleanse.cpp
        )
add_library(UTIL_A STATIC ${BitcoinHeaders} ${UTIL_SOURCES})
target_include_directories(UTIL_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rust/include
        ${OPENSSL_INCLUDE_DIR}
        )

set(CLI_A_SOURCES ./src/rpc/client.cpp)
add_library(CLI_A STATIC ${BitcoinHeaders} ${CLI_A_SOURCES})
target_include_directories(CLI_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
        )

set(SAPLING_SOURCES
        ./src/bech32.cpp
        ./src/sapling/sapling_util.cpp
        ./src/sapling/key_io_sapling.cpp
        ./src/destination_io.cpp
        ./src/sapling/sapling_core_write.cpp
        ./src/sapling/prf.cpp
        ./src/sapling/noteencryption.cpp
        ./src/sapling/address.cpp
        ./src/sapling/note.cpp
        ./src/sapling/zip32.cpp
        ./src/sapling/crypter_sapling.cpp
        ./src/sapling/incrementalmerkletree.cpp
        ./src/sapling/transaction_builder.cpp
        ./src/sapling/saplingscriptpubkeyman.cpp
        ./src/sapling/proof.cpp
        ./src/sapling/sapling_operation.cpp
        )

add_library(SAPLING_A STATIC ${BitcoinHeaders} ${SAPLING_SOURCES})
target_include_directories(SAPLING_A PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sapling
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rust/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/univalue/include
        ${OPENSSL_INCLUDE_DIR}
        ${BerkeleyDB_INCLUDE_DIRS}
        )

link_directories(
        ${CMAKE_SOURCE_DIR}/src/univalue/.libs
        ${CMAKE_SOURCE_DIR}/src/secp256k1/.libs
        ${CMAKE_SOURCE_DIR}/target/release
        )

link_directories(${CMAKE_BINARY_DIR})

set(c_note-cli_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/c_note-cli.cpp)
if($ENV{target} MATCHES "Windows")
    list(APPEND c_note-cli_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/c_note-cli-res.rc)
endif()
add_executable(c_note-cli ${c_note-cli_SOURCES})
add_dependencies(c_note-cli libunivalue libzcashrust)
target_link_libraries(c_note-cli
        CLI_A
        univalue
        UTIL_A
        BITCOIN_CRYPTO_A
        SAPLING_A
        rustzcash
        ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${LIBEVENT_LIB} ${sodium_LIBRARY_RELEASE} -ldl pthread
        )
if($ENV{target} MATCHES "Windows")
    target_link_libraries(c_note-cli ${WINDOWS_LDADD})
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(c_note-cli "-framework Cocoa")
endif()

set(c_note-tx_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/c_note-tx.cpp)
if($ENV{target} MATCHES "Windows")
    list(APPEND c_note-tx_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/c_note-tx-res.rc)
endif()
add_executable(c_note-tx ${c_note-tx_SOURCES})
add_dependencies(c_note-tx libunivalue libsecp256k1 libzcashrust)
target_link_libraries(c_note-tx
        univalue
        COMMON_A
        UTIL_A
        BITCOIN_CRYPTO_A
        secp256k1
        SAPLING_A
        rustzcash
        ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${LIBEVENT_LIB} ${sodium_LIBRARY_RELEASE} -ldl pthread
        )
if($ENV{target} MATCHES "Windows")
    target_link_libraries(c_note-tx ${WINDOWS_LDADD})
endif()
if(GMP_FOUND)
    target_link_libraries(c_note-tx ${GMP_LIBRARY})
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(c_note-tx "-framework Cocoa")
endif()

set(c_noted_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/c_noted.cpp)
if($ENV{target} MATCHES "Windows")
    list(APPEND c_noted_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/c_noted-res.rc)
endif()
add_executable(c_noted ${c_noted_SOURCES} ${BitcoinHeaders})
add_dependencies(c_noted libunivalue libsecp256k1 libzcashrust leveldb crc32c)
target_include_directories(c_noted PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv
        ${LIBEVENT_INCLUDE_DIR})
target_link_libraries(c_noted
        pthread
        SERVER_A
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
        ${BerkeleyDB_LIBRARIES} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${LIBEVENT_LIB} pthread
        )
if($ENV{target} MATCHES "Windows")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wstack-protector -fstack-protector-all -fPIE -pipe -O2 -pthread -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va -pie --static")
    target_link_libraries(c_noted ${WINDOWS_LDADD})
endif()
if(GMP_FOUND)
    target_link_libraries(c_noted ${GMP_LIBRARY})
    target_include_directories(c_noted PUBLIC ${GMP_INCLUDE_DIR})
endif()
if(ZMQ_FOUND)
    target_link_libraries(c_noted ZMQ_A ${ZMQ_LIB})
    target_include_directories(c_noted PUBLIC ${ZMQ_INCLUDE_DIR})
endif()
if(MINIUPNP_FOUND)
    target_compile_definitions(c_noted PUBLIC "-DSTATICLIB -DMINIUPNP_STATICLIB")
    target_link_libraries(c_noted ${MINIUPNP_LIBRARY})
    target_include_directories(c_noted PUBLIC ${MINIUPNP_INCLUDE_DIR})
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(c_noted "-framework Cocoa")
endif()

target_link_libraries(c_noted ${sodium_LIBRARY_RELEASE} -ldl -lpthread)

add_subdirectory(src/qt)
add_subdirectory(src/test)
