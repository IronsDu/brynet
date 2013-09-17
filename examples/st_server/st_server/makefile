st_server:
	gcc -fPIC -c -pg -I../../../common -I../../../server-model02 st_server.c
	gcc -fPIC -c -pg -I../../../common -I../../../server-model02 ../../../server-model02/server.c
	gcc -fPIC -c -pg -I../../../common -I../../../server-model02 ../../../server-model02/epollserver.c
	gcc -shared -g st_server.o server.o epollserver.o ./libcommon.so -o libstserver.so
