#include "db.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	db_t db;

        if (argc != 2) {
                fprintf(stderr, "usage: %s [dbfile]\n", argv[0]);
                return 0;
        }

	if (db_open(&db, argv[1], 0) != DB_OK) {
                fprintf(stderr, "open db %s failed\n", argv[1]);
                return 0;
        }
		
	/*
	db_stat(&db);
	*/

	db_close(&db);

	return 0;
}

