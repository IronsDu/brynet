cmake_minimum_required (VERSION 2.6)
project(brynet)

if(WIN32)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
elseif(UNIX)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()

set(net_src_list
	${PROJECT_SOURCE_DIR}/src/net/EventLoop.cpp
	${PROJECT_SOURCE_DIR}/src/net/CurrentThread.cpp
	${PROJECT_SOURCE_DIR}/src/net/DataSocket.cpp
	${PROJECT_SOURCE_DIR}/src/net/SocketLibFunction.c
	${PROJECT_SOURCE_DIR}/src/net/TCPService.cpp
	${PROJECT_SOURCE_DIR}/src/net/WrapTCPService.cpp
)

set(timer_src_list
	${PROJECT_SOURCE_DIR}/src/timer/Timer.cpp
)

set(utils_src_list
	${PROJECT_SOURCE_DIR}/src/utils/array.c
	${PROJECT_SOURCE_DIR}/src/utils/buffer.c
	${PROJECT_SOURCE_DIR}/src/utils/systemlib.c
)

include_directories("${PROJECT_SOURCE_DIR}/src/net")
include_directories("${PROJECT_SOURCE_DIR}/src/timer")
include_directories("${PROJECT_SOURCE_DIR}/src/utils")

add_executable(pingpongserver ${net_src_list} ${timer_src_list} ${utils_src_list} ${PROJECT_SOURCE_DIR}/examples/PingPongServer.cpp)
if(WIN32)
  target_link_libraries(pingpongserver ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(pingpongserver Threads::Threads)
endif()

add_executable(pingpongclient ${net_src_list} ${timer_src_list} ${utils_src_list} ${PROJECT_SOURCE_DIR}/examples/PingPongClient.cpp)
if(WIN32)
  target_link_libraries(pingpongclient ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(pingpongclient Threads::Threads)
endif()