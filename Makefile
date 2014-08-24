CC = cc

CFLAGS = -O2 -Wall -Werror -Wno-long-long -ansi -pedantic

SRC = hash.c db.c

OBJ = $(SRC:.c=.o)

all: db-put db-get db-bench db-stat

db-put: db-put.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-get: db-get.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-bench: db-bench.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

db-stat: db-stat.c $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf db-put db-get db-bench db-stat *.o *.dSYM
