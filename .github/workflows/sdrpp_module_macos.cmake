# macOS-compatible CMake module for SDR++ plugins
# Based on .github/workflows/sdrpp_module_hack.cmake with macOS fixes

# Get needed values depending on if this is in-tree or out-of-tree
if (NOT SDRPP_CORE_ROOT)
    set(SDRPP_CORE_ROOT "../SDRPlusPlus/core/src")
endif ()
if (NOT SDRPP_LIB_ROOT)
    set(SDRPP_LIB_ROOT "/usr/lib")
endif ()

# Use CMAKE_CXX_STANDARD instead of compiler flags to avoid applying C++ flags to C files
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags (without -std=c++17 since we use CMAKE_CXX_STANDARD)
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_MODULE_COMPILER_FLAGS -g -Og -Wno-unused-command-line-argument)
    else ()
        set(SDRPP_MODULE_COMPILER_FLAGS -g -Og)
    endif ()
else()
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SDRPP_MODULE_COMPILER_FLAGS -O3 -Wno-unused-command-line-argument)
    else ()
        set(SDRPP_MODULE_COMPILER_FLAGS -O3)
    endif ()
endif()

# Created shared lib and link to core
add_library(${PROJECT_NAME} SHARED ${SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE "${SDRPP_LIB_ROOT}/libsdrpp_core.dylib")

# Use -undefined dynamic_lookup for macOS to handle symbols resolved at runtime
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "-undefined dynamic_lookup"
    PREFIX ""
    SUFFIX ".dylib"
)

# Include directories
# SDRPP_CORE_ROOT points to core/src, so no need for extra /src/
target_include_directories(${PROJECT_NAME} PRIVATE
    "${SDRPP_CORE_ROOT}/"
    "${SDRPP_CORE_ROOT}/imgui/"
)

# Homebrew include paths (for volk, fftw, talloc)
if (EXISTS "/opt/homebrew/include")
    target_include_directories(${PROJECT_NAME} PRIVATE "/opt/homebrew/include")
endif()

# Set compile arguments
target_compile_options(${PROJECT_NAME} PRIVATE ${SDRPP_MODULE_COMPILER_FLAGS})

# Find and link talloc
find_package(PkgConfig REQUIRED)
pkg_check_modules(TALLOC REQUIRED talloc)
target_include_directories(${PROJECT_NAME} PRIVATE ${TALLOC_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${TALLOC_LIBRARIES})
target_link_directories(${PROJECT_NAME} PRIVATE ${TALLOC_LIBRARY_DIRS})

# Install directives (for macOS, install to user's Library)
install(TARGETS ${PROJECT_NAME} DESTINATION lib/sdrpp/plugins)
