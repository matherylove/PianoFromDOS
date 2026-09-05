cmake_minimum_required(VERSION 3.16)
project(PianoFromDOS LANGUAGES CXX RC)

# PianoFromDOS is intentionally kept close to the original 2010 C++ code.
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(NOT WIN32)
    message(FATAL_ERROR "PianoFromDOS must be cross-compiled for 32-bit Windows.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "PianoFromDOS targets Windows 98 and must be built as 32-bit x86.")
endif()

set(PFD_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/PianoFromAbove")

set(PFD_SOURCES
    ${PFD_SRC_DIR}/Compat.cpp
    ${PFD_SRC_DIR}/Config.cpp
    ${PFD_SRC_DIR}/ConfigProcs.cpp
    ${PFD_SRC_DIR}/GameState.cpp
    ${PFD_SRC_DIR}/MainProcs.cpp
    ${PFD_SRC_DIR}/MIDI.cpp
    ${PFD_SRC_DIR}/Misc.cpp
    ${PFD_SRC_DIR}/PianoFromAbove.cpp
    ${PFD_SRC_DIR}/Renderer.cpp
    ${PFD_SRC_DIR}/MetaData.cpp
    ${PFD_SRC_DIR}/TinyXML/tinystr.cpp
    ${PFD_SRC_DIR}/TinyXML/tinyxml.cpp
    ${PFD_SRC_DIR}/TinyXML/tinyxmlerror.cpp
    ${PFD_SRC_DIR}/TinyXML/tinyxmlparser.cpp
    ${PFD_SRC_DIR}/PianoFromDOS.rc
)

add_executable(PianoFromDOS WIN32 ${PFD_SOURCES})

target_include_directories(PianoFromDOS PRIVATE
    ${PFD_SRC_DIR}
    ${PFD_SRC_DIR}/TinyXML
)

target_compile_definitions(PianoFromDOS PRIVATE
    UNICODE
    _UNICODE
    WIN32
    _WINDOWS
    TIXML_USE_STL
    PFD_TARGET_WIN98=1
    WINVER=0x0410
    _WIN32_WINDOWS=0x0410
    _WIN32_IE=0x0500
)

# Force the compatibility shim before each C++ translation unit so the original
# MSVC secure-CRT spellings and Win9x aliases are consistently available.
target_compile_options(PianoFromDOS PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:-include${PFD_SRC_DIR}/Compat.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wall>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wextra>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>"
)

# MSLU's import library must appear before the ordinary Win32 import libraries.
# The GitHub Actions workflow builds this library from the pinned libunicows source.
set(PFD_UNICOWS_LIBRARY "" CACHE FILEPATH "Path to libunicows.a")
if(NOT PFD_UNICOWS_LIBRARY OR NOT EXISTS "${PFD_UNICOWS_LIBRARY}")
    message(FATAL_ERROR "PFD_UNICOWS_LIBRARY must point to the built libunicows.a")
endif()

# D3DX9_30 belongs to the Win98-capable DirectX 9.0c generation.  Use the
# target compiler's own import-library search path; CI verifies the final PE
# imports d3dx9_30.dll and never silently accepts a newer d3dx9 runtime.

# Static GCC/libstdc++ removes modern runtime DLL dependencies from the artifact.
target_link_options(PianoFromDOS PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
    -mwindows
)

target_link_libraries(PianoFromDOS PRIVATE
    "${PFD_UNICOWS_LIBRARY}"
    d3d9
    d3dx9_30
    winmm
    comctl32
    advapi32
    msimg32
    shell32
    ole32
    uuid
    comdlg32
    gdi32
    user32
    kernel32
)

set_target_properties(PianoFromDOS PROPERTIES
    OUTPUT_NAME "PianoFromDOS"
    SUFFIX ".exe"
)

# A small, deterministic package layout for CI artifacts.
add_custom_command(TARGET PianoFromDOS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/package"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:PianoFromDOS>" "${CMAKE_BINARY_DIR}/package/PianoFromDOS.exe"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/WIN98_RUNTIME.txt" "${CMAKE_BINARY_DIR}/package/WIN98_RUNTIME.txt"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Docs/License.txt" "${CMAKE_BINARY_DIR}/package/Original-PianoFromAbove-License.txt"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt" "${CMAKE_BINARY_DIR}/package/THIRD_PARTY_NOTICES.txt"
)
