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
	db_iter_t iter;

	char key[1024];
	char val[1024];
	uint32_t klen, vlen;
	uint64_t len;

	if (argc != 3) {
		fprintf(stderr, "usage: %s [datafile] [indexfile]\n", argv[0]);
		return 0;
	}

	if (db_open(&db, argv[1], argv[2], 256, 256, 0) != DB_OK) {
		fprintf(stderr, "open db %s failed\n", argv[1]);
		return 0;
	}

	if (db_iter(&db, &iter, NULL, 0) != DB_OK) {
		fprintf(stderr, "iter db %s failed\n", argv[1]);
		return 0;
	}
	
	len = 0;
	klen = sizeof(key);
	vlen = sizeof(val);
	while (db_iter_next(&db, &iter, key, &klen, val, &vlen) == DB_OK) {
		char buf[1024];
		memcpy(buf,                   key, klen);
		memcpy(buf + klen,            ": ", 2);
		memcpy(buf + klen + 2,        val, vlen);
		memcpy(buf + klen + 2 + vlen, "\n", 1);
		fwrite(buf, sizeof(char), klen + vlen + 3, stdout);
		klen = sizeof(key);
		vlen = sizeof(val);
		len++;
	}
	
	printf("len: %llu\n", len);
	db_close(&db);

	return 0;
}

