cmake_minimum_required(VERSION 3.13)
project(tetra_demodulator)

set(CMAKE_INSTALL_PREFIX "/usr/")
include(GNUInstallDirs)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

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

add_library(tetra_demodulator SHARED ${SRC})
target_link_libraries(tetra_demodulator PRIVATE sdrpp_core osmocore)
target_include_directories(tetra_demodulator PRIVATE "src/" "src/decoder/src" "src/decoder/codec" "/usr/include/sdrpp_core/" "/usr/include/sdrpp_core/imgui")
set_target_properties(tetra_demodulator PROPERTIES PREFIX "")

# Install directives
install(TARGETS tetra_demodulator DESTINATION /usr/lib/sdrpp/plugins)
