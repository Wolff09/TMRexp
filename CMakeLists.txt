cmake_minimum_required(VERSION 2.6)
project(TMRexp CXX)


# verbose output
set(CMAKE_VERBOSE_MAKEFILE OFF)

# don't get rid of assertions
# set(CMAKE_BUILD_TYPE Debug)
# set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_BUILD_TYPE Release)

# use c++11
if(CMAKE_COMPILER_IS_GNUCXX)
	add_definitions(-fno-strict-aliasing)
	add_definitions(-Wall -Wextra)
	add_definitions(-Wno-unused-parameter -Wno-unused-function)
	add_definitions(-fshow-column)
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++17")
endif(CMAKE_COMPILER_IS_GNUCXX)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	add_definitions(-fno-strict-aliasing)
	add_definitions(-Wall -Wextra)
	add_definitions(-Wno-unused-parameter -Wno-unused-function)
	# add_definitions(-fshow-column)
	add_definitions(-ferror-limit=5)
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++17") #  -ftemplate-depth=512
endif()


# Boost
FIND_PACKAGE( Boost )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )


# adds test target
enable_testing()

# dedicated folder for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(BIN_PATH ${CMAKE_BINARY_DIR}/bin)


# includes
include_directories(src)
include_directories(test)


# sources/executables
add_subdirectory(src)
add_subdirectory(test)
