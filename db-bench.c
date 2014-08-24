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

	uint8_t key[1024];
	uint8_t val[1024];

	uint32_t klen, vlen;

        struct timeval tv;
	uint32_t start, end;
	uint32_t size = 0;

        if (argc != 3) {
                fprintf(stderr, "usage: %s [dbfile] [loop]\n", argv[0]);
                return 0;
        }

	if (db_open(&db, argv[1], 0) != DB_OK) {
                fprintf(stderr, "open db %s failed\n", argv[1]);
                return 0;
        }

	loop = atoi(argv[2]);

	memset(val, 0, sizeof(val));

	vlen = sizeof(vlen);
        gettimeofday(&tv, NULL);
        start = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	for (i = 0; i < loop; i++) {
		int ret;
		klen = sprintf((char *)key, "%016d", i);
		vlen = sprintf((char *)val, "%0100d", i);
		ret = db_put(&db, key, klen, val, vlen);
		if (ret != DB_OK) {
			printf("set key: %s\n", key);
			break;
		}
		size += klen + vlen;
	}
	gettimeofday(&tv, NULL);
        end = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	printf("write: %6.3f MB/s\n", size / 1024.0/1024.0 /(end - start) * 1000);

	memset(val, 0, sizeof(val));

	size = 0;
	start = end;
	for (i = 0; i < loop; i++) {
		int ret;
		klen = sprintf((char *)key, "%016d", i);
		ret = db_get(&db, key, klen, val, sizeof(val));
		size += klen + 100;
		if (ret == 0) {
			printf("get key: %s\n", key);
			break;
		}
	}

	gettimeofday(&tv, NULL);
        end = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	printf("read: %6.3f MB/s\n", size / 1024.0/1024.0 /(end - start) * 1000);
	
	db_close(&db);

	return 0;
}

