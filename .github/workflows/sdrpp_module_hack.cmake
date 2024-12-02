# Get needed values depending on if this is in-tree or out-of-tree
if (NOT SDRPP_CORE_ROOT)
    set(SDRPP_CORE_ROOT "../sdrpp_lib/SDRPlusPlus-master/core/src")
endif ()
if (NOT SDRPP_LIB_ROOT)
    set(SDRPP_LIB_ROOT "/usr/lib")
endif ()
if (NOT SDRPP_MODULE_COMPILER_FLAGS)
    # Compiler flags
    if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
        # Debug Flags
        if (MSVC)
            set(SDRPP_MODULE_COMPILER_FLAGS /std:c++17 /EHsc)
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(SDRPP_MODULE_COMPILER_FLAGS -g -Og -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
        else ()
            set(SDRPP_MODULE_COMPILER_FLAGS -g -Og -std=c++17)
        endif ()
    else()
        # Normal Flags
        if (MSVC)
            set(SDRPP_MODULE_COMPILER_FLAGS /O2 /Ob2 /std:c++17 /EHsc )
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set(SDRPP_MODULE_COMPILER_FLAGS -O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup)
        else ()
            set(SDRPP_MODULE_COMPILER_FLAGS -O3 -std=c++17)
        endif ()
    endif()
endif ()

# Created shared lib and link to core
add_library(${PROJECT_NAME} SHARED ${SRC})
if (MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${SDRPP_LIB_ROOT}/sdrpp_core.lib")
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE "${SDRPP_LIB_ROOT}/libsdrpp_core.so")
endif()
target_include_directories(${PROJECT_NAME} PRIVATE "${SDRPP_CORE_ROOT}/src/" "${SDRPP_CORE_ROOT}/src/imgui/")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

# Set compile arguments
target_compile_options(${PROJECT_NAME} PRIVATE ${SDRPP_MODULE_COMPILER_FLAGS})

# if (MSVC)
#     target_link_options(${PROJECT_NAME} PRIVATE /FORCE:UNRESOLVED)
# endif ()

# Install directives
install(TARGETS ${PROJECT_NAME} DESTINATION lib/sdrpp/plugins)

if (MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif ()

if (MSVC)
    # Lib path
    target_link_directories(${PROJECT_NAME} PUBLIC "C:/Program Files/PothosSDR/lib/")

    # Misc headers
    target_include_directories(${PROJECT_NAME} PUBLIC "C:/Program Files/PothosSDR/include/")

    # Volk
    target_link_libraries(${PROJECT_NAME} PUBLIC volk)

    # OpenGL
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenGL::GL)

    # GLFW3
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC glfw)

    # FFTW3
    find_package(FFTW3f CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC FFTW3::fftw3f)


    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    add_compile_definitions(_WINSOCKAPI_)
    # WinSock2
    target_link_libraries(${PROJECT_NAME} PUBLIC wsock32 ws2_32 iphlpapi)

    # ZSTD
    find_package(zstd CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC zstd::libzstd_shared)
endif ()
