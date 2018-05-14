cmake_minimum_required (VERSION 2.6)
project(brynet)

if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
elseif(UNIX)
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
	${PROJECT_SOURCE_DIR}/src/brynet/net/WrapTCPService.cpp
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
	${PROJECT_SOURCE_DIR}/src/brynet/utils/array.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/buffer.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/app_status.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/ox_file.cpp
)

include_directories("${PROJECT_SOURCE_DIR}/src/")
if(WIN32)
  add_library(brynet ${net_src_list} ${timer_src_list} ${http_src_list} ${utils_src_list})
elseif(UNIX)
  add_library(brynet SHARED ${net_src_list} ${timer_src_list} ${http_src_list} ${utils_src_list})
  #add_library(brynet STATIC ${net_src_list} ${timer_src_list} ${http_src_list} ${utils_src_list})
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

add_subdirectory(examples)

if(WIN32)
  if (MSVC_VERSION VERSION_GREATER 1910)
    add_subdirectory(tests)
  endif()
elseif(UNIX)
  if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7.0) 
    add_subdirectory(tests)
  endif()
endif()