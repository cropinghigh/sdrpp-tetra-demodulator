cmake_minimum_required(VERSION 3.13)
project(tetra_crypto_tests C)

include(CTest)

add_executable(tetra_crypto_test
    crypto_test.c
    ../src/decoder/src/crypto/tetra_crypto.c
    ../src/decoder/src/crypto/tea1.c
    ../src/decoder/src/crypto/tea2.c
    ../src/decoder/src/crypto/tea3.c
    ../src/decoder/src/crypto/taa1.c
    ../src/decoder/src/crypto/hurdle.c
    ../src/decoder/src/tetra_common.c)

target_include_directories(tetra_crypto_test PRIVATE
    ../src/decoder/src
    ../src/decoder/codec)

if(MSVC)
    target_compile_options(tetra_crypto_test PRIVATE /UNDEBUG)
else()
    target_compile_options(tetra_crypto_test PRIVATE -UNDEBUG)
endif()

add_test(NAME tetra_crypto_test COMMAND tetra_crypto_test)
