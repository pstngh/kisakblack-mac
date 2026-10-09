# Standalone, non-Windows build of the OpenAL audio backend + a smoke test. This is
# the audio analogue of src/gfx_gl/CMakeLists.txt: it lets the XAudio2→OpenAL layer be
# built and run-tested in isolation while it is brought up.
#   cmake -S src/audio_openal -B build_al && cmake --build build_al && ./build_al/al_smoke
cmake_minimum_required(VERSION 3.16)
project(kisak_al CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED WINSDK_INC)
    set(WINSDK_INC "${CMAKE_CURRENT_SOURCE_DIR}/../platform/winsdk")
endif()

if(APPLE)
    # Homebrew's openal-soft (keg-only), not the system's deprecated OpenAL.framework.
    execute_process(COMMAND brew --prefix openal-soft OUTPUT_VARIABLE OPENAL_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    find_library(OPENAL_LIB openal PATHS "${OPENAL_PREFIX}/lib" NO_DEFAULT_PATH REQUIRED)
    set(OPENAL_INC "${OPENAL_PREFIX}/include")
else()
    find_library(OPENAL_LIB openal REQUIRED)
endif()

add_library(kisak_al STATIC al_audio.cpp)
target_include_directories(kisak_al PUBLIC
    "${WINSDK_INC}" "${CMAKE_CURRENT_SOURCE_DIR}" ${OPENAL_INC})
if(APPLE)
    # <malloc.h> stand-in, and on arm64 the x86 SSE headers as sse2neon wrappers.
    target_include_directories(kisak_al BEFORE PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../platform/compat/darwin")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        execute_process(COMMAND brew --prefix OUTPUT_VARIABLE BREW_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        target_include_directories(kisak_al BEFORE PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/../platform/compat/arm64" "${BREW_PREFIX}/include/sse2neon")
    endif()
endif()
target_compile_options(kisak_al PRIVATE
    -fms-extensions -fpermissive -w
    -include "${CMAKE_CURRENT_SOURCE_DIR}/../platform/compat/msvc_compat.h" -DKISAK_MP)
target_link_libraries(kisak_al PUBLIC ${OPENAL_LIB})

add_executable(al_smoke tests/al_smoke.cpp)
target_link_libraries(al_smoke PRIVATE kisak_al)
target_compile_options(al_smoke PRIVATE
    -fms-extensions -fpermissive -w
    -include "${CMAKE_CURRENT_SOURCE_DIR}/../platform/compat/msvc_compat.h" -DKISAK_MP)
