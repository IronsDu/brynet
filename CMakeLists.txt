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
endif()

set(net_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/net/EventLoop.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/CurrentThread.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/DataSocket.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/SocketLibFunction.c
	${PROJECT_SOURCE_DIR}/src/brynet/net/TCPService.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/WrapTCPService.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/Connector.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/ListenThread.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/fdset.c
	${PROJECT_SOURCE_DIR}/src/brynet/net/SSLHelper.cpp
)

set(timer_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/timer/Timer.cpp
)

set(base_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/utils/array.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/buffer.c
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
)

include_directories("${PROJECT_SOURCE_DIR}/src/")

add_executable(pingpongserver ${net_src_list} ${timer_src_list} ${base_src_list} ${PROJECT_SOURCE_DIR}/examples/PingPongServer.cpp)
if(WIN32)
  target_link_libraries(pingpongserver ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(pingpongserver pthread)
endif()

add_executable(pingpongclient ${net_src_list} ${timer_src_list} ${base_src_list} ${PROJECT_SOURCE_DIR}/examples/PingPongClient.cpp)
if(WIN32)
  target_link_libraries(pingpongclient ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(pingpongclient pthread)
endif()

add_executable(broadcastserver ${net_src_list} ${timer_src_list} ${base_src_list} ${PROJECT_SOURCE_DIR}/examples/BroadCastServer.cpp)
if(WIN32)
  target_link_libraries(broadcastserver ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(broadcastserver pthread)
endif()

add_executable(broadcastclient ${net_src_list} ${timer_src_list} ${base_src_list} ${PROJECT_SOURCE_DIR}/examples/BroadCastClient.cpp)
if(WIN32)
  target_link_libraries(broadcastclient ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(broadcastclient pthread)
endif()

add_executable(httpserver ${net_src_list} ${timer_src_list} ${base_src_list} ${utils_src_list} ${http_src_list} ${PROJECT_SOURCE_DIR}/examples/TestHttp.cpp)
if(WIN32)
  target_link_libraries(httpserver ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(httpserver pthread)
endif()

add_executable(benchwebsocket ${net_src_list} ${timer_src_list} ${base_src_list} ${utils_src_list} ${http_src_list} ${PROJECT_SOURCE_DIR}/examples/BenchWebsocket.cpp)
if(WIN32)
  target_link_libraries(benchwebsocket ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(benchwebsocket pthread)
endif()

add_executable(promisereceive ${net_src_list} ${timer_src_list} ${base_src_list} ${utils_src_list} ${http_src_list} ${PROJECT_SOURCE_DIR}/examples/TestPromiseReceive.cpp)
if(WIN32)
  target_link_libraries(promisereceive ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(promisereceive pthread)
endif()

add_executable(web_binary_proxy ${net_src_list} ${timer_src_list} ${base_src_list} ${utils_src_list} ${http_src_list} ${PROJECT_SOURCE_DIR}/examples/WebBinaryProxy.cpp)
if(WIN32)
  target_link_libraries(web_binary_proxy ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(web_binary_proxy pthread)
endif()