#
# Copyright (c) Moonsik Park. All rights reserved.
#
# @file   CMakeLists.txt
# @author Moonsik Park, Korea Institute of Science and Technology
#

cmake_minimum_required(VERSION 3.19)

PROJECT(ngp-encode-server
	VERSION 1.0
	DESCRIPTION "NGP encode server"
	LANGUAGES C CXX
)

set(NES_VERSION "${CMAKE_PROJECT_VERSION}")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "No release type specified. Setting to 'Release'.")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include_directories("include")
include_directories("dependencies")
include_directories("dependencies/tinylogger")

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVFILTER_INCLUDE_DIR libavfilter/avfilter.h)
find_library(AVFILTER_LIBRARY avfilter)

find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h)
find_library(SWSCALE_LIBRARY swscale)

find_package(Freetype REQUIRED)
find_package(websocketpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)

add_subdirectory(proto)

include_directories(${AVCODEC_INCLUDE_DIR})
include_directories(${AVFORMAT_INCLUDE_DIR})
include_directories(${AVUTIL_INCLUDE_DIR})
include_directories(${SWSCALE_INCLUDE_DIR})
include_directories(${FREETYPE_INCLUDE_DIRS})
include_directories(${WEBSOCKETPP_INCLUDE_DIR})
include_directories(${CMAKE_BINARY_DIR}/proto)

set(SOURCES
	src/encode.cpp
	src/server.cpp
	src/main.cpp
	src/base/camera_manager.cc
	src/base/server/camera_control.cc
	src/base/server/packet_stream.cc
	src/base/server/websocket_server.cc
	src/base/video/frame_queue.cc
	src/base/video/frame_map.cc
	src/base/video/type_managers.cc
	src/base/video/render_text.cc
	src/base/video/rendered_frame.cc
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})

add_executable(neserver src/main.cpp ${SOURCES})
target_include_directories(neserver PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVFILTER_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${SWSCALE_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS} ${WEBSOCKETPP_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
target_link_libraries(neserver PRIVATE proto ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVFILTER_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${FREETYPE_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto)
