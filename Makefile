CC = cc

CFLAGS = -Wall -Werror -Wno-long-long -ansi -pedantic -g

SRC = hash.c db.c
OBJ = $(SRC:.c=.o)

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
        CFLAGS += -D_BSD_SOURCE 
        CFLAGS += -D_GNU_SOURCE
        CFLAGS += -D_POSIX_SOURCE 
        CFLAGS += -D_XOPEN_SOURCE 
        CFLAGS += -D_POSIX_C_SOURCE=200809L 
        CFLAGS += -DLINUX
endif

all: db-put db-get db-del db-iter db-stat db-export db-import db-bench db-server

db-put: db-put.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-get: db-get.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-del: db-del.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-iter: db-iter.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-stat: db-stat.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-export: db-export.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-import: db-import.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-bench: db-bench.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-server: db-server.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf db-put db-get db-del db-iter db-stat db-export db-import db-bench db-server *.o *.dSYM
