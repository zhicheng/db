#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	db_t db;
	char *val;
	size_t vlen = 0;

	if (argc != 3 && argc != 4) {
		fprintf(stderr, "usage: %s [dbfile] [key] [val]\n", argv[0]);
		fprintf(stderr, "or   : %s [dbfile] [key] < [file]\n", argv[0]);
		return 0;
	}

	if (db_open(&db, argv[1], 0) != DB_OK) {
		fprintf(stderr, "open db %s failed\n", argv[1]);
		return 0;
	}

	if (argc == 3) {
		int n = 0;
		vlen = 4096;
		val = malloc(vlen);
		while ((val[n++] = getchar()) != EOF) {
			if (n >= vlen) {
				vlen *= 2;
				val = realloc(val, vlen);
			}
		}
		vlen = n - 1;
	} else {
		vlen = strlen(argv[3]);
		val = malloc(vlen);
		memcpy(val, argv[3], vlen);
	}
	
	if (db_put(&db, argv[2], strlen(argv[2]), val, vlen) == DB_OK) {
		fprintf(stderr, "OK\n");
	} else {
		fprintf(stderr, "NOT OK\n");
	}
	
	db_close(&db);

	return 0;
}

