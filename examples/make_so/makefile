VPATH	=	../../common
CFLAGS	=	-I./../../common/
objects = array.o buffer.o list.o fdset.o mutex.o \
	rwlist.o socketlibfunction.o stack.o systemlib.o \
	thread.o timeaction.o typepool.o threadpool.o multipool.o \
	heap.o connection.o
TARGET	=	libcommon.so
common	:	$(objects)
	gcc -shared -O3 -pg $(objects) -o $(TARGET)
clean	:
	rm $(objects) -rf
	rm $(TARGET) -rf
%.o	:	%.c
	gcc -fPIC $(CFLAGS)  -c -g -O3 -Wall $< 
