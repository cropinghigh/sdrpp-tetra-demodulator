cmake_minimum_required(VERSION 3.13)
project(tetra_demodulator)

set(CMAKE_INSTALL_PREFIX "/usr/")
include(GNUInstallDirs)

file(GLOB SRC LIST_DIRECTORIES false 
    "src/*.cpp"
    "src/*.c"
    "src/dsp/*.cpp"
    "src/decoder/src/*.cpp"
    "src/decoder/src/*.c"
    "src/decoder/src/crypto/*.cpp"
    "src/decoder/src/crypto/*.c"
    "src/decoder/src/lower_mac/*.cpp"
    "src/decoder/src/lower_mac/*.c"
    "src/decoder/src/phy/*.cpp"
    "src/decoder/src/phy/*.c"
    "src/decoder/codec/c-code/cdec_tet.c"
    "src/decoder/codec/c-code/sub_cd.c"
    "src/decoder/codec/c-code/tetra_op.c"
    "src/decoder/codec/c-code/sdec_tet.c"
    "src/decoder/codec/c-code/sub_sc_d.c"
    "src/decoder/codec/c-code/sub_dsp.c"
    "src/decoder/codec/c-code/fbas_tet.c"
    "src/decoder/codec/c-code/fexp_tet.c"
    "src/decoder/codec/c-code/fmat_tet.c"
    "src/decoder/codec/c-code/tetra_op.c"
    )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

if (NOT SDRPP_MODULE_CMAKE)
    set(SDRPP_MODULE_CMAKE "/usr/share/cmake/Modules/sdrpp_module.cmake")
endif ()


include(${SDRPP_MODULE_CMAKE})

if(MSVC)
  # target_compile_options(tetra_demodulator PRIVATE /W4 /WX)
else()
  # target_compile_options(tetra_demodulator PRIVATE -Wall -Wextra)
endif()

target_include_directories(tetra_demodulator PRIVATE BEFORE "src/" "src/decoder/src" "src/decoder/codec" )
