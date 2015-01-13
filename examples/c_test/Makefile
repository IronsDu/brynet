server:
	gcc -I../common/ -I../net/ ../common/*.c ../net/*.c test_server.c -lpthread -lrt -o pingpong_server

client:
	gcc -I../common/ -I../net/ ../common/*.c ../net/*.c pingpang_client.c -lpthread -lrt -o pingpong_client
