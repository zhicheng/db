#ifndef __DB_H__
#define __DB_H__

#include <stdint.h>
#include <stdlib.h>

enum {DB_SYS_ERROR = -1, DB_ERROR = 0, DB_OK = 1};

typedef struct db_table {
        uint64_t bucket_off;	/* offset in file	*/
        uint64_t bucket_key;	/* key in use		*/
        uint64_t bucket_len;	/* buckets in table	*/
} db_table_t;

typedef struct db_bucket {
        uint64_t hash;		/* key hash     	*/
        uint64_t off;		/* offset in file	*/
} db_bucket_t;

typedef struct db_iter {
	uint64_t table_off;
	uint64_t bucket_off;
} db_iter_t;

typedef struct db_stat {
	uint64_t db_file_size;

	uint64_t db_table_max;
	uint64_t db_table_min;

	uint64_t db_table_total;
	uint64_t db_table_size;

	uint64_t db_bucket_total;
	uint64_t db_bucket_size;

	uint64_t db_data_size;
} db_stat_t;

/* disk format */
typedef struct db_file_header {
	uint32_t magic;
	uint32_t version;
	uint64_t data_head;
	uint64_t data_tail;
	uint64_t table_off;
	uint64_t table_len;
} db_file_header_t;

typedef struct db_file {
	struct db *db;

	void	 *buf;
        uint64_t  buflen;

	int	 fd;
	int	 pgsz;
	uint64_t size;
        int      rdonly;

	db_file_header_t *header;
} db_file_t;


typedef struct db {
	int 	   db_mode;
	int 	   db_error;

	db_file_t *db_index;
	db_file_t *db_data;

	db_file_t db_file_index;
	db_file_t db_file_data;

	uint64_t db_table_len;
} db_t;

typedef struct db_option {
	uint64_t table;
	uint64_t bucket;
	uint64_t rdonly;
} db_option_t;

/*
 * if index is NULL or same data
 * is the single file mode (mixin data and index)
 */
int
db_open(db_t *db, const char *data, const char *index, const db_option_t *option);

int
db_put(db_t *db, const void *key, uint32_t klen, const void *val, uint32_t vlen);

/*
 * if value size bigger than vlen, val will fill in value 0 ~ vlen 
 * return value length
 */
uint32_t
db_get(db_t *db, const void *key, uint32_t klen, void *val, uint32_t vlen);

int
db_del(db_t *db, const void *key, uint32_t klen);

int
db_iter(db_t *db, db_iter_t *iter, const void *key, const uint32_t klen);

/* 
 * klen is pointer of key buffer length
 * vlen is pointer of val buffer length
 *
 * when function finish: 
 * klen will set db's key length
 * vlen will set db's val length
 */
int
db_iter_next(db_t *db, db_iter_t *iter,
	void *key, uint32_t *klen, void *val, uint32_t *vlen);

int
db_stat(db_t *db, db_stat_t *stat);

int
db_close(db_t *db);


#endif /* __DB_H__ */
