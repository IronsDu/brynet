cmake_minimum_required (VERSION 2.6)
project(brynet)

find_library(LIB_OPENSSL NAMES crypto ssl)
if(LIB_OPENSSL)
  add_definitions(-DUSE_OPENSSL)
  SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-as-needed -ldl -lrt -lssl -lcrypto")
elseif()
  message("not found openssl")
endif()

if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
elseif(UNIX)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()
    SET(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g -ggdb -Wall -D_DEBUG")
    SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -ggdb -Wall -DNDEBUG")
  if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -fPIC")
endif()

set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")

set(net_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/net/EventLoop.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/CurrentThread.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/DataSocket.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/SocketLibFunction.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/TCPService.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/Connector.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/ListenThread.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/fdset.c
	${PROJECT_SOURCE_DIR}/src/brynet/net/SSLHelper.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/Socket.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/SyncConnector.cpp
)

set(timer_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/timer/Timer.cpp
)

set(http_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/net/http/http_parser.c
	${PROJECT_SOURCE_DIR}/src/brynet/net/http/HttpParser.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/http/HttpService.cpp
)

set(utils_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/utils/base64.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/utils/md5calc.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/utils/SHA1.cpp
    ${PROJECT_SOURCE_DIR}/src/brynet/utils/stack.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/array.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/buffer.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/app_status.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/ox_file.cpp
)

include_directories("${PROJECT_SOURCE_DIR}/src/")
if(WIN32)
	add_library(brynet ${net_src_list} ${timer_src_list} ${http_src_list} ${utils_src_list})
	set_target_properties(brynet PROPERTIES OUTPUT_NAME_DEBUG "brynetd")
elseif(UNIX)
  add_library(brynet STATIC ${net_src_list} ${timer_src_list} ${http_src_list} ${utils_src_list})
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

option(brynet_BUILD_EXAMPLES "Build examples" OFF)
option(brynet_BUILD_TESTS "Build test" OFF)

if (brynet_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif(brynet_BUILD_EXAMPLES)

if (brynet_BUILD_TESTS)
  if(WIN32)
    if (MSVC_VERSION VERSION_GREATER 1910)
      add_subdirectory(tests)
    endif() 
  elseif(UNIX)
    if(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0) 
      add_subdirectory(tests)
    endif() 
  endif() 
endif(brynet_BUILD_TESTS)
