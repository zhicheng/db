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

	char val[1024];
	uint32_t vlen;

	if (argc != 4) {
		fprintf(stderr, "usage: %s [databfile] [indexfile] [key]\n", argv[0]);
		return 0;
	}

        option.rdonly = 1;
	if (db_open(&db, argv[1], argv[2], &option) != DB_OK) {
		fprintf(stderr, "open db %s failed\n", argv[1]);
		return 0;
	}
	
	if ((vlen = db_get(&db, argv[3], strlen(argv[3]), val, sizeof(val))) != 0) {
		fwrite(val, sizeof(char), vlen, stdout);
	} else {
		fprintf(stderr, "NOT FOUND\n");
	}

	db_close(&db);

	return 0;
}

