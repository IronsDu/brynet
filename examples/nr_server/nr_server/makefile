nr_server:
	gcc -fPIC -c -pg -I../../../common -I../../../server-model02 ../../../server-model02/thread_reactor.c
	gcc -fPIC -c -pg -I../../../common -I../../../server-model02 ../../../server-model02/server.c
	gcc -fPIC -c -pg -I../../../common -I../../../server-model02 ../../../server-model02/epollserver.c
	gcc -shared -g thread_reactor.o server.o epollserver.o ./libcommon.so -o libnrserver.so
