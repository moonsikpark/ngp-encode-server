#
#  Copyright (c) Moonsik Park. All rights reserved.
#
#  @file   CMakeLists.txt
#  @author Moonsik Park, Korean Institute of Science and Technology
#

cmake_minimum_required(VERSION 3.19)

PROJECT(ngp-encode-server
	VERSION 1.0
	DESCRIPTION "NGP encode server"
	LANGUAGES C CXX
)


set(NES_VERSION "${CMAKE_PROJECT_VERSION}")


if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "No release type specified. Setting to 'Release'.")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

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

include_directories(${AVCODEC_INCLUDE_DIR})
include_directories(${AVFORMAT_INCLUDE_DIR})
include_directories(${AVUTIL_INCLUDE_DIR})
include_directories(${SWSCALE_INCLUDE_DIR})
include_directories(${FREETYPE_INCLUDE_DIRS})

list(APPEND NES_DEFINITIONS -DNES_VERSION="${NES_VERSION}")

include_directories("include")

set(SOURCES
	src/encode.cpp
	src/server.cpp
	src/pov.cpp
	src/main.cpp
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})

add_executable(neserver src/main.cpp)
add_library(nes STATIC ${SOURCES})
target_link_libraries(neserver PUBLIC nes)
target_compile_definitions(neserver PUBLIC ${NES_DEFINITIONS})
target_include_directories(neserver PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVFILTER_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${SWSCALE_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(neserver PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVFILTER_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY} ${FREETYPE_LIBRARIES})
