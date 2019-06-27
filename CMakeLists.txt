cmake_minimum_required (VERSION 2.6)
project(brynet)

if(APPLE)
  set(CMAKE_MACOSX_RPATH 0)
endif()

if(NOT APPLE)
  find_library(LIB_OPENSSL NAMES crypto ssl)
  if(LIB_OPENSSL)
    add_definitions(-DUSE_OPENSSL)
    SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-as-needed -ldl -lrt -lssl -lcrypto")
  elseif()
    message("not found openssl")
  endif()
endif()

if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
elseif(UNIX)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()
    SET(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g -ggdb -Wall -Wextra -D_DEBUG")
    SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -ggdb -Wall -Wextra -DNDEBUG")
  if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fPIC")
endif()

set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")

file(GLOB_RECURSE ALL_SRC 
  ${PROJECT_SOURCE_DIR}/src/*.cpp 
  ${PROJECT_SOURCE_DIR}/src/*.cc 
  ${PROJECT_SOURCE_DIR}/src/*.c)

include_directories("${PROJECT_SOURCE_DIR}/src/")
if(WIN32)
	add_library(brynet ${ALL_SRC})
	set_target_properties(brynet PROPERTIES OUTPUT_NAME_DEBUG "brynetd")
elseif(UNIX)
  add_library(brynet SHARED ${ALL_SRC})
  find_package(Threads REQUIRED)
  target_link_libraries(brynet pthread)
endif()
install(TARGETS brynet DESTINATION lib)

file(GLOB BRYNET_NET_PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/src/brynet/net/*.h)
install (FILES ${BRYNET_NET_PUBLIC_HEADERS} DESTINATION "include/brynet/net")

file(GLOB BRYNET_TIMER_PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/src/brynet/timer/*.h)
install (FILES ${BRYNET_TIMER_PUBLIC_HEADERS} DESTINATION "include/brynet/timer")

file(GLOB BRYNET_UTILS_PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/src/brynet/utils/*.h)
install (FILES ${BRYNET_UTILS_PUBLIC_HEADERS} DESTINATION "include/brynet/utils")

file(GLOB BRYNET_HTTP_PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/src/brynet/net/http/*.h)
install (FILES ${BRYNET_HTTP_PUBLIC_HEADERS} DESTINATION "include/brynet/net/http")

file(GLOB BRYNET_PORT_PUBLIC_HEADERS ${PROJECT_SOURCE_DIR}/src/brynet/net/port/*.h)
install (FILES ${BRYNET_PORT_PUBLIC_HEADERS} DESTINATION "include/brynet/net/port")

option(brynet_BUILD_EXAMPLES "Build examples" OFF)
option(brynet_BUILD_TESTS "Build test" OFF)

if (brynet_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif(brynet_BUILD_EXAMPLES)

if (brynet_BUILD_TESTS)
  if(WIN32)
    if (MSVC_VERSION VERSION_GREATER 1900)
      add_subdirectory(tests)
    endif() 
  elseif(UNIX)
    add_subdirectory(tests)
  endif() 
endif(brynet_BUILD_TESTS)
