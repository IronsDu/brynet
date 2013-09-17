OBJECTS	=	test_timer.o
main	:	$(OBJECTS)	
	gcc $(OBJECTS) ./common.so -lpthread -lrt -o ./main
clean	:	
	rm $(OBJECTS) ./main -rf
%.o	:	%.c
	gcc -c $<
