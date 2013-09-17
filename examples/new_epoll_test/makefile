VPATH	=	../../common ../../server-model02
CFLAGS	=	-I./../../common/ -I./../../server-model02
objects = array.o buffer.o list.o fdset.o mutex.o \
	rwlist.o socketlibfunction.o stack.o systemlib.o \
	thread.o timeaction.o typepool.o threadpool.o \
	server.o epollserver.o heap.o msgnet.o thread_reactor.o
TARGET	=	common.so
common	:	$(objects)
	gcc -shared -O3 -pg $(objects) -o $(TARGET)
clean	:
	rm $(objects) -rf
	rm $(TARGET) -rf
%.o	:	%.c
	gcc -fPIC $(CFLAGS)  -c -g -O3 -Wall $< 
