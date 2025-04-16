MANAGER_OBJS = fss_manager.o sync_info_lookup.o hashtable_path.o hashtable_watchdesc.o list.o string.o queue.o
MANAGER_OUT = fss_manager
WORKER_OBJS = worker.o
WORKER_OUT = worker
SOURCE	= fss_manager.c sync_info_lookup.c hashtable_path.c hashtable_watchdesc.c string.c list.c queue.c worker.c
HEADER  = utils.h sync_info_lookup.h hashtable_path.h hashtable_watchdesc.h string.h list.h worker.h queue.h
CC = gcc
FLAGS = -g -c -Wall

all: $(MANAGER_OUT) $(WORKER_OUT)

$(MANAGER_OUT): $(MANAGER_OBJS)
	$(CC) -g -o $(MANAGER_OUT) $(MANAGER_OBJS)

$(WORKER_OUT): $(WORKER_OBJS)
	$(CC) -g -o $(WORKER_OUT) $(WORKER_OBJS)


fss_manager.o: fss_manager.c sync_info_lookup.h utils.h string.h worker.h queue.h
	$(CC) $(FLAGS) fss_manager.c

worker.o: worker.c
	$(CC) $(FLAGS) worker.c

list.o: list.c list.h string.h utils.h
	$(CC) $(FLAGS) list.c

string.o: string.c string.h
	$(CC) $(FLAGS) string.c

hashtable_path.o: hashtable_path.c hashtable_path.h list.h string.h utils.h
	$(CC) $(FLAGS) hashtable_path.c

hashtable_watchdesc.o: hashtable_watchdesc.c hashtable_watchdesc.h list.h string.h utils.h
	$(CC) $(FLAGS) hashtable_watchdesc.c

sync_info_lookup.o: sync_info_lookup.c hashtable_path.h hashtable_watchdesc.h list.h string.h utils.h
	$(CC) $(FLAGS) sync_info_lookup.c

queue.o: queue.c queue.h
	$(CC) $(FLAGS) queue.c


clean:
	rm -f $(MANAGER_OBJS) $(MANAGER_OUT) worker.o worker


count:
	wc $(SOURCE) $(HEADER)
