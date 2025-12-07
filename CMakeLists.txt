cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(ReplayBuffer VERSION 1.0.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES}
        src/AudioRecorder.cpp
        src/Timer.cpp
        src/RBSettingsLayer.cpp
        src/Recorder.cpp)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

if(ANDROID)
    find_package(ffmpeg REQUIRED)
elseif(WIN32)
    if(NOT DEFINED FFMPEG_ROOT)
        set(FFMPEG_ROOT "D:/ffmpeg-prebuilt" CACHE PATH "Path to FFmpeg from gyan.dev")
    endif()

    set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
    set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")

    find_library(AVCODEC_LIBRARY avcodec PATHS ${FFMPEG_LIB_DIR} REQUIRED)
    find_library(AVFORMAT_LIBRARY avformat PATHS ${FFMPEG_LIB_DIR} REQUIRED)
    find_library(AVUTIL_LIBRARY avutil PATHS ${FFMPEG_LIB_DIR} REQUIRED)
    find_library(SWSCALE_LIBRARY swscale PATHS ${FFMPEG_LIB_DIR} REQUIRED)
    find_library(SWRESAMPLE_LIBRARY swresample PATHS ${FFMPEG_LIB_DIR} REQUIRED)
    find_library(AVFILTER_LIBRARY avfilter PATHS ${FFMPEG_LIB_DIR} REQUIRED)

    set(FFMPEG_LIBRARIES
            ${AVCODEC_LIBRARY}
            ${AVFORMAT_LIBRARY}
            ${AVUTIL_LIBRARY}
            ${SWSCALE_LIBRARY}
            ${SWRESAMPLE_LIBRARY}
            ${AVFILTER_LIBRARY}
            ws2_32
            secur32
            strmiids
            bcrypt
            mfplat
            mfuuid
            ncrypt
            crypt32
    )

    message(STATUS ${FFMPEG_LIBRARIES})
elseif(APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
            libavcodec
            libavformat
            libavutil
            libswscale
            libswresample
            libavfilter
    )
    set(FFMPEG_LIBRARIES PkgConfig::FFMPEG)
    set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${FFMPEG_LIBRARIES})

setup_geode_mod(${PROJECT_NAME})
