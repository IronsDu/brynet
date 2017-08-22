cmake_minimum_required (VERSION 2.6)
project(brynet)

if(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
elseif(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()

set(net_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/net/EventLoop.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/CurrentThread.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/DataSocket.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/SocketLibFunction.c
	${PROJECT_SOURCE_DIR}/src/brynet/net/TCPService.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/WrapTCPService.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/Connector.cpp
	${PROJECT_SOURCE_DIR}/src/brynet/net/fdset.c
)

set(timer_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/timer/Timer.cpp
)

set(base_src_list
	${PROJECT_SOURCE_DIR}/src/brynet/utils/array.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/buffer.c
	${PROJECT_SOURCE_DIR}/src/brynet/utils/systemlib.c
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
  target_link_libraries(pingpongserver Threads::Threads)
endif()

add_executable(pingpongclient ${net_src_list} ${timer_src_list} ${base_src_list} ${PROJECT_SOURCE_DIR}/examples/PingPongClient.cpp)
if(WIN32)
  target_link_libraries(pingpongclient ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(pingpongclient Threads::Threads)
endif()

add_executable(broadcastserver ${net_src_list} ${timer_src_list} ${base_src_list} ${PROJECT_SOURCE_DIR}/examples/BroadCastServer.cpp)
if(WIN32)
  target_link_libraries(broadcastserver ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(broadcastserver Threads::Threads)
endif()

add_executable(broadcastclient ${net_src_list} ${timer_src_list} ${base_src_list} ${PROJECT_SOURCE_DIR}/examples/BroadCastClient.cpp)
if(WIN32)
  target_link_libraries(broadcastclient ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(broadcastclient Threads::Threads)
endif()

add_executable(httpserver ${net_src_list} ${timer_src_list} ${base_src_list} ${utils_src_list} ${http_src_list} ${PROJECT_SOURCE_DIR}/examples/TestHttp.cpp)
if(WIN32)
  target_link_libraries(httpserver ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(httpserver Threads::Threads)
endif()

add_executable(benchwebsocket ${net_src_list} ${timer_src_list} ${base_src_list} ${utils_src_list} ${http_src_list} ${PROJECT_SOURCE_DIR}/examples/BenchWebsocket.cpp)
if(WIN32)
  target_link_libraries(benchwebsocket ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(benchwebsocket Threads::Threads)
endif()

add_executable(promisereceive ${net_src_list} ${timer_src_list} ${base_src_list} ${utils_src_list} ${http_src_list} ${PROJECT_SOURCE_DIR}/examples/TestPromiseReceive.cpp)
if(WIN32)
  target_link_libraries(promisereceive ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(promisereceive Threads::Threads)
endif()