include_directories("${PROJECT_SOURCE_DIR}/include/")

FILE(GLOB_RECURSE HEADER_LIST
*.h
*.hpp
../include/brynet/base/*.h
../include/brynet/base/*.hpp
../include/brynet/base/endian/*.h
../include/brynet/base/endian/*.hpp
../include/brynet/base/crypto/*.h
../include/brynet/base/crypto/*.hpp
../include/brynet/net/*.h 
../include/brynet/net/*.hpp
../include/brynet/net/detail/*.h
../include/brynet/net/detail/*.hpp
../include/brynet/net/http/*.h
../include/brynet/net/http/*.hpp
../include/brynet/net/port/*.h
../include/brynet/net/port/*.hpp
../include/brynet/net/wrapper/*.h
../include/brynet/net/wrapper/*.hpp )

add_executable(pingpongserver PingPongServer.cpp ${HEADER_LIST})
if(WIN32)
  target_link_libraries(pingpongserver ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(pingpongserver pthread)
endif()

add_executable(pingpongclient PingPongClient.cpp)
if(WIN32)
  target_link_libraries(pingpongclient ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(pingpongclient pthread)
endif()

add_executable(broadcastserver BroadCastServer.cpp)
if(WIN32)
  target_link_libraries(broadcastserver ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(broadcastserver pthread)
endif()

add_executable(broadcastclient BroadCastClient.cpp)
if(WIN32)
  target_link_libraries(broadcastclient ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(broadcastclient pthread)
endif()

add_executable(httpserver HttpServer.cpp)
if(WIN32)
  target_link_libraries(httpserver ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(httpserver pthread)
  if(LIB_OPENSSL)
    target_link_libraries(httpserver ssl crypto)
  endif()
endif()

add_executable(httpclient HttpClient.cpp)
if(WIN32)
  target_link_libraries(httpclient ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(httpclient pthread)
  if(LIB_OPENSSL)
    target_link_libraries(httpclient ssl crypto)
  endif()
endif()

add_executable(benchwebsocket BenchWebsocket.cpp)
if(WIN32)
  target_link_libraries(benchwebsocket ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(benchwebsocket pthread)
endif()

add_executable(promisereceive PromiseReceive.cpp)
if(WIN32)
  target_link_libraries(promisereceive ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(promisereceive pthread)
endif()

add_executable(webbinaryproxy WebBinaryProxy.cpp)
if(WIN32)
  target_link_libraries(webbinaryproxy ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(webbinaryproxy pthread)
endif()