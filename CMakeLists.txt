cmake_minimum_required(VERSION 3.10)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mingw-toolchain.cmake" CACHE STRING "Toolchain file for MinGW")

project(MGSFPSUnlock VERSION 1.0 LANGUAGES CXX)

if(MINGW)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m64 -static-libstdc++")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")

add_library(MGSFPSUnlock SHARED
    MGSFPSUnlock/dllmain.cpp
    MGSFPSUnlock/Memory.cpp
    MGSFPSUnlock/MGS2/mgs2.cpp
    MGSFPSUnlock/MGS3/mgs3.cpp
    MGSFPSUnlock/Utils.cpp
)

set(MINHOOK_BUILD_SHARED OFF CACHE BOOL "Build MinHook as a shared library" FORCE)
set(MINHOOK_BUILD_STATIC ON CACHE BOOL "Build MinHook as a static library" FORCE)
add_subdirectory(external/minhook)

target_include_directories(MGSFPSUnlock PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/MGSFPSUnlock/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/MGSFPSUnlock/MGS2
    ${CMAKE_CURRENT_SOURCE_DIR}/MGSFPSUnlock/MGS3
    ${CMAKE_CURRENT_SOURCE_DIR}/external/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/minhook/include
)

target_compile_definitions(MGSFPSUnlock PRIVATE
    _WINDOWS
    _USRDLL
    MGS3FPSUNLOCK_EXPORTS
    NDEBUG
)

target_compile_options(MGSFPSUnlock PRIVATE -fpermissive)

set_target_properties(MGSFPSUnlock PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    PREFIX ""
    OUTPUT_NAME "MGSFPSUnlock"
    SUFFIX ".asi"
)

target_link_libraries(MGSFPSUnlock PRIVATE
    shlwapi
    version
    minhook
)
