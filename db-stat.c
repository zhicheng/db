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
	db_stat_t stat;

	if (argc != 3) {
		fprintf(stderr, "usage: %s [datafile] [indexfile]\n", argv[0]);
		return 0;
	}

	if (db_open(&db, argv[1], argv[2], 0, 0, 0) != DB_OK) {
		fprintf(stderr, "open db %s failed\n", argv[1]);
		return 0;
	}

	if (db_stat(&db, &stat) != DB_OK) {
		fprintf(stderr, "db_stat error\n");
	}

        printf("db_file_size: %llu\n", stat.db_file_size);
        printf("db_table_max: %llu\n", stat.db_table_max);
        printf("db_table_min: %llu\n", stat.db_table_min);
        printf("db_table_total: %llu\n", stat.db_table_total);
        printf("db_table_size: %llu\n", stat.db_table_size);
        printf("db_bucket_total: %llu\n", stat.db_bucket_total);
        printf("db_bucket_size: %llu\n", stat.db_bucket_size);
        printf("db_data_size: %llu\n", stat.db_data_size);
	
	db_close(&db);

	return 0;
}

