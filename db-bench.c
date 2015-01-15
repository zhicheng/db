#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

int
main(int argc, char *argv[])
{
	int i;
	int loop;

	db_t db;
	db_option_t option;

	uint8_t key[1024];
	uint8_t val[1024];

	uint32_t klen, vlen;

        struct timeval tv;
	uint64_t start, end;
	uint64_t size = 0;

        if (argc != 4) {
                fprintf(stderr, "usage: %s [datafile] [indexfile] [loop]\n", argv[0]);
                return 0;
        }

        option.table  = 256;
        option.bucket = 256;
        option.rdonly = 0;
	if (db_open(&db, argv[1], argv[2], &option) != DB_OK) {
                fprintf(stderr, "open db %s failed\n", argv[1]);
                return 0;
        }

	loop = atoi(argv[3]);

	memset(val, 0, sizeof(val));

	vlen = sizeof(vlen);
        gettimeofday(&tv, NULL);
        start = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	for (i = 0; i < loop; i++) {
		klen = sprintf((char *)key, "%016d", i);
		vlen = sprintf((char *)val, "%0128d", i);
		if (db_put(&db, key, klen, val, vlen) != DB_OK) {
			printf("db_put error: %s\n", key);
			break;
		}
		size += klen + vlen;
	}
	gettimeofday(&tv, NULL);
        end = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	printf("write: %6.3f MB/s\n", size / 1024.0/1024.0 /(end - start) * 1000);
	printf("write: %6.3f Keys/s\n",  (float)loop /(end - start) * 1000);

	memset(val, 0, sizeof(val));

	size = 0;
	start = end;
	for (i = 0; i < loop; i++) {
		klen = sprintf((char *)key, "%016d", i);
		vlen = db_get(&db, key, klen, val, sizeof(val));
		size += klen + vlen;
		if (vlen == 0) {
			printf("db_get error: %s\n", key);
			break;
		}
	}

	gettimeofday(&tv, NULL);
        end = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	printf("read: %6.3f MB/s\n", size / 1024.0/1024.0 /(end - start) * 1000);
	printf("read: %6.3f Keys/s\n",  (float)loop /(end - start) * 1000);
	
	db_close(&db);

	return 0;
}

