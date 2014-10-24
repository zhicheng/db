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
	db_option_t option;

	char *val;
	size_t vlen = 0;

	if (argc != 4 && argc != 5) {
		fprintf(stderr, "usage: %s [datafile] [indexfile] [key] [val]\n", argv[0]);
		fprintf(stderr, "or   : %s [datafile] [indexfile] [key] < [file]\n", argv[0]);
		return 0;
	}

	option.table  = 256;
	option.bucket = 256;
	option.rdonly = 0;
	if (db_open(&db, argv[1], argv[2], &option) != DB_OK) {
		fprintf(stderr, "open db %s failed\n", argv[1]);
		return 0;
	}

	if (argc == 4) {
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
		vlen = strlen(argv[4]);
		val = malloc(vlen);
		memcpy(val, argv[4], vlen);
	}
	
	if (db_put(&db, argv[3], strlen(argv[3]), val, vlen) == DB_OK) {
		fprintf(stderr, "OK\n");
	} else {
		fprintf(stderr, "NOT OK\n");
	}
	
	db_close(&db);

	return 0;
}

