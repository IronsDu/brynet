enable_testing()

include_directories("${PROJECT_SOURCE_DIR}/include/")

add_executable(test_timer test_timer.cpp)
add_test(TestTimer test_timer)

add_executable(test_wait_group test_wait_group.cpp)
if(WIN32)
  target_link_libraries(test_wait_group ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(test_wait_group pthread)
endif()
add_test(TestWaitGroup test_wait_group)

add_executable(test_sync_connect test_sync_connect.cpp)
if(WIN32)
  target_link_libraries(test_sync_connect ws2_32)
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(test_sync_connect pthread)
endif()
add_test(TestSyncConnect test_sync_connect)

add_executable(test_array test_array.cpp)
add_test(TestArray test_array)

add_executable(test_buffer test_buffer.cpp)
add_test(TestBuffer test_buffer)

add_executable(test_stack test_stack.cpp)
add_test(TestStack test_stack)

add_executable(test_packet test_packet.cpp)
if(WIN32)
  target_link_libraries(test_packet ws2_32)
endif()
add_test(TestPacket test_packet)

add_executable(test_endian test_endian.cpp)
if(WIN32)
  target_link_libraries(test_endian ws2_32)
endif()
add_test(TestEndian test_endian)