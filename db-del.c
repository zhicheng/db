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

	if (argc != 4) {
		fprintf(stderr, "usage: %s [datafile] [indexfile] [key]\n", argv[0]);
		return 0;
	}

        option.table  = 256;
        option.bucket = 256;
        option.rdonly = 0;
	if (db_open(&db, argv[1], argv[2], &option) != DB_OK) {
		fprintf(stderr, "open db %s failed\n", argv[1]);
		return 0;
	}

	if (db_del(&db, argv[3], strlen(argv[3])) == DB_OK) {
		fprintf(stderr, "OK\n");
	} else {
		fprintf(stderr, "NOT OK\n");
	}
	
	db_close(&db);

	return 0;
}

