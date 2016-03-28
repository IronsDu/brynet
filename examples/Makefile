CFLAGS = -O3 -std=c++0x
LIBS = -lrt
INCLUDES = -I./../src/net -I./../src/utils -I./../src/timer
COMMON_SRCS = ./../src/timer/timer.cpp ./../src/net/CurrentThread.cpp ./../src/net/EventLoop.cpp ./../src/net/WrapTCPService.cpp ./../src/net/DataSocket.cpp ./../src/net/TCPService.cpp
COMMON_C_SRCS = ./../src/net/SocketLibFunction.c  ./../src/utils/systemlib.c ./../src/utils/buffer.c
COMMON_TARGETS = $(COMMON_SRCS:.cpp=.o) $(COMMON_C_SRCS:.c=.o)

BROADCASTSERVER_TARGETS = BroadCastServer.o

broadcastserver: $(COMMON_TARGETS) $(BROADCASTSERVER_TARGETS)
	g++ $(COMMON_TARGETS) $(BROADCASTSERVER_TARGETS) $(LIBS) -o broadcastserver
%.o: %.cpp,%.c
	g++ $(CFLAGS) $(INCLUDES) -c $< -o $@
