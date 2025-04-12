MANAGER_OBJS = fss_manager.o hashtable.o list.o string.o
SOURCE	= fss_manager.c hashtable.c string.c list.c
HEADER  = utils.h hashtable.h string.h list.h
MANAGER_OUT = fss_manager
CC = gcc
FLAGS = -g -c -Wall

$(MANAGER_OUT): $(MANAGER_OBJS)
	$(CC) -g -o $(MANAGER_OUT) $(MANAGER_OBJS)
 
fss_manager.o: fss_manager.c hashtable.h utils.h string.h
	$(CC) $(FLAGS) fss_manager.c

list.o: list.c list.h string.h utils.h
	$(CC) $(FLAGS) list.c

string.o: string.c string.h
	$(CC) $(FLAGS) string.c

hashtable.o: hashtable.c hashtable.h list.h string.h utils.h
	$(CC) $(FLAGS) hashtable.c


clean:
	rm -f $(MANAGER_OBJS) $(MANAGER_OUT)


count:
	wc $(SOURCE) $(HEADER)
