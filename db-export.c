#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int  i;
	db_t db;
	db_iter_t iter;
	db_option_t option;

	uint32_t item;
	uint32_t klen;
	uint32_t vlen;

	char key[1024];
	char val[1024];

	FILE *fp;

	if (argc != 4) {
		fprintf(stderr, "usage: %s [datafile] [indexfile] [file]\n", argv[0]);
		return 0;
	}

        option.rdonly = 1;
	if (db_open(&db, argv[1], argv[2], &option) != DB_OK) {
		fprintf(stderr, "open db %s failed\n", argv[1]);
		return 0;
	}

	if (db_iter(&db, &iter, NULL, 0) != DB_OK) {
		fprintf(stderr, "iter db %s failed\n", argv[1]);
		return 0;
	}

	fp = fopen(argv[3], "w");
	if (fp == NULL) {
		fprintf(stderr, "open file %s failed\n", argv[3]);
		return 0;
	}

	item = 0;
	klen = sizeof(key);
	vlen = sizeof(val);
	while (db_iter_next(&db, &iter, key, &klen, val, &vlen) == DB_OK) {
		for (i = 0; i < klen; i++)
			fprintf(fp, "%02x", key[i]);
		fprintf(fp, "\n");
		for (i = 0; i < vlen; i++)
			fprintf(fp, "%02x", val[i]);
		fprintf(fp, "\n");

		klen = sizeof(key);
		vlen = sizeof(val);
		item += 1;
	}

	printf("%u", item);
	
	fclose(fp);
	db_close(&db);

	return 0;
}

