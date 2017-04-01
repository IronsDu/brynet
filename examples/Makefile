CFLAGS = -O3 -std=c++0x
LIBS = -lrt -lpthread
INCLUDES = -I./../src/net -I./../src/utils -I./../src/timer -I./../src/net/http
COMMON_SRCS =	./../src/timer/Timer.cpp\
		./../src/net/CurrentThread.cpp\
		./../src/net/EventLoop.cpp\
		./../src/net/WrapTCPService.cpp\
		./../src/net/DataSocket.cpp\
		./../src/net/TCPService.cpp
COMMON_C_SRCS = ./../src/net/SocketLibFunction.c\
		./../src/utils/systemlib.c\
		./../src/utils/buffer.c

HTTP_COMMON_SRCS = ./../src/net/http/HttpParser.cpp\
			./../src/net/http/HttpServer.cpp\
			./../src/utils/base64.cpp\
			./../src/utils/SHA1.cpp
HTTP_COMMON_C_SRCS = ./../src/net/http/http_parser.c\

COMMON_TARGETS = $(COMMON_SRCS:.cpp=.o) $(COMMON_C_SRCS:.c=.o)
HTTP_COMMON_TARGETS = $(HTTP_COMMON_SRCS:.cpp=.o) $(HTTP_COMMON_C_SRCS:.c=.o)

PINGPONGSERVER_TARGETS = PingPongServer.o
PINGPONGCLIENT_TARGETS = PingPongClient.o
BROADCASTSERVER_TARGETS = BroadCastServer.o
BROADCASTCLIENT_TARGETS = BroadCastClient.o
TESTHTTP_TARGETS =	TestHttp.o
BENCH_WEBSOCKET_TARGETS = BenchWebsocket.o
			
all :	pingpongserver\
	pingpongclient\
	broadcastserver\
	broadcastclient\
	httpserver\
	benchwebsocket

pingpongserver: $(COMMON_TARGETS) $(PINGPONGSERVER_TARGETS)
	$(CXX) $(COMMON_TARGETS) $(PINGPONGSERVER_TARGETS) $(LIBS) -o pingpongserver
pingpongclient: $(COMMON_TARGETS) $(PINGPONGCLIENT_TARGETS)
	$(CXX) $(COMMON_TARGETS) $(PINGPONGCLIENT_TARGETS) $(LIBS) -o pingpongclient
broadcastserver: $(COMMON_TARGETS) $(BROADCASTSERVER_TARGETS)
	$(CXX) $(COMMON_TARGETS) $(BROADCASTSERVER_TARGETS) $(LIBS) -o broadcastserver
broadcastclient: $(COMMON_TARGETS) $(BROADCASTCLIENT_TARGETS)
	$(CXX) $(COMMON_TARGETS) $(BROADCASTCLIENT_TARGETS) $(LIBS) -o broadcastclient
httpserver: $(COMMON_TARGETS) $(TESTHTTP_TARGETS) $(HTTP_COMMON_TARGETS)
	$(CXX) $(COMMON_TARGETS) $(TESTHTTP_TARGETS) $(HTTP_COMMON_TARGETS) $(LIBS) -o httpserver
benchwebsocket: $(COMMON_TARGETS) $(BENCH_WEBSOCKET_TARGETS) $(HTTP_COMMON_TARGETS)
	$(CXX) $(COMMON_TARGETS) $(BENCH_WEBSOCKET_TARGETS) $(HTTP_COMMON_TARGETS) $(LIBS) -o benchwebsocket

%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@
%.o: %.c
	$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@
clean : 
	find ./../ -name "*.o"  | xargs rm -f
