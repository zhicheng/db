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

        option.table  = 256;
        option.bucket = 256;
        option.rdonly = 0;
	if (db_open(&db, argv[1], argv[2], &option) != DB_OK) {
		fprintf(stderr, "open db %s failed\n", argv[1]);
		return 0;
	}

	fp = fopen(argv[3], "r");
	if (fp == NULL) {
		fprintf(stderr, "open file %s failed\n", argv[3]);
		return 0;
	}

	item = 0;
	klen = 0;
	vlen = 0;
	while (!feof(fp)) {
		char    *line = NULL;
		size_t   linecap = 0;
		ssize_t  linelen;

		memset(key, 0, sizeof(key));
		if ((linelen = getline(&line, &linecap, fp)) > 0) {
			linelen -= linelen % 2;
			for (i = 0; i < linelen; i+=2) {
				char str[3];
				str[2] = '\0';
				memcpy(str, line + i, 2);
				key[klen++] = strtol(str, NULL, 16);
			}
		}
		line = NULL;
		linecap = 0;
		memset(val, 0, sizeof(val));
		if ((linelen = getline(&line, &linecap, fp)) > 0) {
			linelen -= linelen % 2;
			for (i = 0; i < linelen; i+=2) {
				char str[3];
				str[2] = '\0';
				memcpy(str, line + i, 2);
				val[vlen++] = strtol(str, NULL, 16);
			}
		}
		if (klen) {
			if (db_put(&db, key, klen, val, vlen) == DB_OK) {
				item += 1;
			}
		}
		klen = 0;
		vlen = 0;
	}

	printf("%u", item);
	
	fclose(fp);
	db_close(&db);

	return 0;
}

