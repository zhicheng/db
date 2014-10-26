/*
 * db.c was written by WEI Zhicheng, and is placed in the public domain.
 * The author hereby disclaims copyright to this source code.
 */

#include "db.h"
#include "hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define DB_MAGIC	0x00004244
#define DB_MAGIC_INDEX	0x58494244
#define DB_MAGIC_DATA	0x54444244
#define DB_VERSION	2

#define PAGE_ALIGN(ptr,pgsz)	\
	((char *)(ptr) - (((char *)(ptr) - (char *)NULL) & ((pgsz) - 1)))

static int
db_file_open(db_file_t *file, const char *filename, int rdonly)
{
	if (rdonly)
		file->fd = open(filename, O_RDONLY, 0644);
	else
		file->fd = open(filename, O_RDWR | O_CREAT, 0644);

	if (file->fd == -1)
		return DB_SYS_ERROR;

	file->rdonly = rdonly;

	return DB_OK;
}

static size_t
db_file_size(db_file_t *file)
{
	struct stat stat;

	if (fstat(file->fd, &stat) == -1)
		return 0;
	return stat.st_size;
}

static int
db_file_resize(db_file_t *file, size_t size)
{
	assert(!file->rdonly);

	if (ftruncate(file->fd, size) == -1)
		return DB_SYS_ERROR;
	return DB_OK;
}

static int
db_file_read(db_file_t *file, void *buf, off_t off, size_t len)
{
	assert(off + len <= file->buflen);

	memcpy(buf, (uint8_t *)file->buf + off, len);
	return len;
}

static int
db_file_write(db_file_t *file, const void *buf, off_t off, size_t len)
{
	assert(!file->rdonly);
	assert(off + len <= file->buflen);

	memcpy((uint8_t *)file->buf + off, buf, len);
	return len;
}

static int
db_file_compare(db_file_t *file, const void *buf, off_t off, size_t len)
{
	assert(off + len <= file->buflen);

	return memcmp((uint8_t *)file->buf + off, buf, len);
}

static int 
db_file_sync(db_file_t *file, off_t off, size_t len)
{
	void *ptr;

	assert(file);
	assert(!file->rdonly);

	ptr = (uint8_t *)file->buf + off;
	if (msync(PAGE_ALIGN(ptr, file->pgsz), len, MS_SYNC) == -1) {
		return DB_SYS_ERROR;
	}
	return DB_OK;
}

static int
db_file_mmap(db_file_t *file)
{
	int prot;
	int flags;

	assert(file);

	if (file->buf != NULL) {
		int error;
		if (!file->rdonly &&
		    (error = db_file_sync(file, 0, file->buflen)) != DB_OK)
			return error;
		if (munmap(file->buf, file->buflen) == -1)
			return DB_SYS_ERROR;
	}

	file->size = db_file_size(file);

	if (file->rdonly) {
		prot  = PROT_READ;
		flags = MAP_PRIVATE;
	} else {
		prot  = PROT_READ | PROT_WRITE;
		flags = MAP_SHARED;
	}
	file->buf = mmap(NULL, file->size, prot, flags, file->fd, 0);
	if (file->buf == MAP_FAILED)
		return DB_SYS_ERROR;
	file->buflen = file->size;

	file->header = file->buf;

	return DB_OK;
}

enum {DB_FILE_ADVISE_UNLIKELY = MADV_WILLNEED,
      DB_FILE_ADVISE_LIKELY   = MADV_WILLNEED};

static int
db_file_advise(db_file_t *file, off_t off, size_t len, int advise)
{
	void *ptr;

	ptr = PAGE_ALIGN((uint8_t *)file->buf + off, file->pgsz);
	if (madvise(PAGE_ALIGN(ptr, file->pgsz), len, advise) == -1)
		return DB_SYS_ERROR;
	return DB_OK;
}

#define db_file_likely(file,off,len)	\
	db_file_advise((file), (off), (len), DB_FILE_ADVISE_LIKELY)

#define db_file_unlikely(file,off,len)	\
	db_file_advise((file), (off), (len), DB_FILE_ADVISE_LIKELY)


static uint64_t
db_file_alloc(db_file_t *file, uint64_t len)
{
	int error;
	uint64_t off; 

	assert(!file->rdonly);

	if ((file->header->data_tail + len) > file->size) {

		/* Changeable,time and space tradeoff */
		size_t newsize = (file->header->data_tail + len) * 2;

		if ((error = db_file_resize(file, newsize)) != DB_OK) {
			file->db->db_error = error;
			return 0;
		}
		if ((error = db_file_mmap(file)) != DB_OK) {
			file->db->db_error = error;
			return 0;
		}
	}

	off = file->header->data_tail;
	file->header->data_tail += len;
	return off;
}

static uint64_t
db_file_calloc(db_file_t *file, uint64_t len)
{
	uint64_t off;

	off = db_file_alloc(file, len);
	if (off != 0)
		memset((uint8_t *)file->buf + off, 0, len);

	return off;
}

static int
db_file_init(db_file_t *file, size_t size)
{
	int error;

	file->pgsz = sysconf(_SC_PAGESIZE);

	if ((size > db_file_size(file)) && !file->rdonly)
		if ((error = db_file_resize(file, size)) != DB_OK)
			return error;

	if ((error = db_file_mmap(file)) != DB_OK)
		return error;

	return DB_OK;
}

static int
db_file_close(db_file_t *file)
{
        if (!file->rdonly && msync(file->buf, file->buflen, MS_SYNC) == -1)
		return DB_SYS_ERROR;

        if (munmap(file->buf, file->buflen) == -1)
		return DB_SYS_ERROR;

        if (close(file->fd) == -1)
		return DB_SYS_ERROR;
	return DB_OK;
}

static int
db_table_read(db_t *db, db_table_t *table, uint64_t off)
{
	return db_file_read(db->db_index, table,
		db->db_index->header->table_off + off * sizeof(db_table_t),
		sizeof(db_table_t));
}

static int
db_table_write(db_t *db, db_table_t *table, uint64_t off)
{
	return db_file_write(db->db_index, table,
		db->db_index->header->table_off + off * sizeof(db_table_t),
		sizeof(db_table_t));
}

static int
db_bucket_read(db_t *db, db_table_t *table, db_bucket_t *bucket, uint64_t off)
{
	return db_file_read(db->db_index, bucket,
		table->bucket_off + off * sizeof(db_bucket_t),
		sizeof(db_bucket_t));
}

static int
db_bucket_write(db_t *db, db_table_t *table, db_bucket_t *bucket, uint64_t off)
{
	return db_file_write(db->db_index, bucket,
		table->bucket_off + off * sizeof(db_bucket_t),
		sizeof(db_bucket_t));
}

static int
db_table_resize(db_t *db, uint64_t table_off, uint64_t bucket_per_table)
{
	uint64_t i;
	uint64_t off;

	uint32_t klen;
	uint32_t vlen;

	db_table_t old_table;
	db_table_t new_table;

	vlen = bucket_per_table * sizeof(db_bucket_t);
	off = db_file_calloc(db->db_index, vlen + sizeof(klen) + sizeof(vlen));

	if (off == 0)
		return DB_SYS_ERROR;

	/* make db-data can expert data when single file */
	/* just writer klen = 0, vlen = table size 	 */
	klen = 0;
	off += db_file_write(db->db_index, &klen, off, sizeof(klen));
	off += db_file_write(db->db_index, &vlen, off, sizeof(vlen));

	db_table_read(db, &old_table, table_off);

	new_table.bucket_off = off;
	new_table.bucket_key = old_table.bucket_key;
	new_table.bucket_len = bucket_per_table;

	for (i = 0; i < old_table.bucket_len; i++) {
		uint64_t j;
		db_bucket_t bucket;

		db_bucket_read(db, &old_table, &bucket, i);

		if (bucket.hash == 0)
			continue;

		for (j = bucket.hash % bucket_per_table;;
		     j = (j + 1) % bucket_per_table)
		{
			db_bucket_t new_bucket;

			db_bucket_read(db, &new_table, &new_bucket, j);
			if (new_bucket.hash == 0) {
				db_bucket_write(db, &new_table, &bucket, j);
				break;
			}
		}
	}

	db_table_write(db, &new_table, table_off);

	return DB_OK;
}

static int
db_index_init(db_t *db, uint64_t table, uint64_t bucket)
{
	uint64_t i;
	uint64_t table_off;
	assert(db && db->db_index->buf);

        db->db_index->header->magic      = DB_MAGIC;
        db->db_index->header->version    = DB_VERSION;

	db->db_index->header->data_head  = sizeof(db_file_header_t);
	db->db_index->header->data_tail  = db->db_index->buflen;

	table_off = db_file_calloc(db->db_index, table * sizeof(db_table_t));
	if (table_off == 0) 
		return DB_SYS_ERROR;

	db->db_index->header->table_off = table_off;
	db->db_index->header->table_len = table;

	for (i = 0; i < table; i++) {
		db_table_resize(db, i, bucket);
	}

	return DB_OK;
}

static int
db_data_init(db_t *db)
{
	assert(db && db->db_data->buf);

        db->db_data->header->magic      = DB_MAGIC;
        db->db_data->header->version    = DB_VERSION;

	db->db_data->header->data_head  = sizeof(db_file_header_t);
	db->db_data->header->data_tail  = db->db_data->buflen;

	return DB_OK;
}

int
db_open(db_t *db, const char *data, const char *index, const db_option_t *option)
{
	int init;
	int error;

	memset(db, 0, sizeof(struct db));

	if (index != NULL && strcmp(index, data) == 0)
		index = NULL;

	db->db_data = &db->db_file_data;
	if ((error = db_file_open(db->db_data, data, option->rdonly)) != DB_OK)
		return error;

	if (index != NULL) {		/* separate index and data file */
		db->db_index = &db->db_file_index;
		if ((error = db_file_open(db->db_index, index, option->rdonly)) != DB_OK)
			return error;
	} else {
		db->db_index = &db->db_file_data;
	}

	init  = db_file_size(db->db_index);
	error = db_file_init(db->db_index, sizeof(db_file_header_t));
	if (error != DB_OK)
		return error;

	if (!init && !option->rdonly) {
		error = db_index_init(db, option->table, option->bucket);
		if (error != DB_OK)
			return error;
	}
	db->db_table_len = db->db_index->header->table_len;

	init  = db_file_size(db->db_data);
	error = db_file_init(db->db_data, sizeof(db_file_header_t));
	if (error != DB_OK)
		return error;

	if (!init) {
		if ((error = db_data_init(db)) != DB_OK)
			return error;
	}

	if (db->db_index->header->version != DB_VERSION ||
	    db->db_data->header->version != DB_VERSION)
	{
		return DB_SYS_ERROR;
	}

	if (db->db_index->header->magic != DB_MAGIC && 
	    db->db_index->header->magic != DB_MAGIC_INDEX)
	{
		return DB_SYS_ERROR;
	}

	if (db->db_data->header->magic != DB_MAGIC && 
	    db->db_data->header->magic != DB_MAGIC_DATA)
	{
		return DB_SYS_ERROR;
	}

	db_file_likely(db->db_data, 0, sizeof(*db->db_data->header));

	return DB_OK;
}

int
db_put(db_t *db, const void *key, uint32_t klen, const void *val, uint32_t vlen)
{
	uint64_t i;
	uint64_t len;
	uint64_t data;

	uint64_t    hash;
	db_table_t  table;
	db_bucket_t bucket;

	hash = db_hash(key, klen);
	db_table_read(db, &table, hash % db->db_index->header->table_len);

	if (((table.bucket_key + 1) * 2) > table.bucket_len) {
		if (db_table_resize(db, hash % db->db_table_len,
					table.bucket_len * 2) != DB_OK)
		{
			return DB_SYS_ERROR;
		}
		db_table_read(db, &table, hash % db->db_table_len);
	}

	len  = sizeof(uint32_t) * 2 + klen + vlen;

	data = db_file_alloc(db->db_data, len);
	if (data == 0)
		return DB_SYS_ERROR;

	data += db_file_write(db->db_data, &klen, data, sizeof(uint32_t));
	data += db_file_write(db->db_data, &vlen, data, sizeof(uint32_t));                       
	data += db_file_write(db->db_data, key, data, klen);
	data += db_file_write(db->db_data, val, data, vlen);

	bucket.hash = hash;
	bucket.off  = data - len;

	for (i = hash % table.bucket_len;; i = (i + 1) % table.bucket_len) {
		db_bucket_t db_bucket;

		db_bucket_read(db, &table, &db_bucket, i);
		if (db_bucket.hash != 0) {
			uint64_t koff;

			if (db_bucket.hash != bucket.hash)
				continue;
			if (db_file_compare(db->db_data, &klen, db_bucket.off,
						sizeof(klen)) != 0)
			{
				continue;
			}
			koff = db_bucket.off + sizeof(klen) + sizeof(vlen);
			if (db_file_compare(db->db_data, key, koff, klen) != 0)
				continue;
		}

		db_bucket_write(db, &table, &bucket, i);

		if (db_bucket.hash == 0) {
			table.bucket_key += 1;
			db_table_write(db, &table, hash % db->db_table_len);
		}
		return DB_OK;
	} 

	return DB_SYS_ERROR;
}

uint32_t
db_get(db_t *db, const void *key, uint32_t klen, void *val, uint32_t vlen)
{
	uint64_t i;

	uint64_t    hash;
	db_table_t  table;
	db_bucket_t bucket;

	hash = db_hash(key, klen);
	db_table_read(db, &table, hash % db->db_table_len);

	for (i = hash % table.bucket_len;; i = (i + 1) % table.bucket_len) {
		uint64_t koff;

		db_bucket_read(db, &table, &bucket, i);

		if (bucket.hash == 0)
			break;

		if (bucket.hash != hash)
			continue;

		if (db_file_compare(db->db_data, &klen, bucket.off,
					sizeof(klen)) != 0)
		{
			continue;
		}
	
		koff = bucket.off + sizeof(klen) + sizeof(vlen);
		if (db_file_compare(db->db_data, key, koff, klen) == 0) {
			uint32_t len;

			db_file_read(db->db_data, &len,
					bucket.off + sizeof(klen), sizeof(len));

			if (len < vlen)
				vlen = len;
				
			db_file_read(db->db_data, val, koff + klen, vlen);
			return len;
		}
	}

	return 0;
}

int
db_del(db_t *db, const void *key, uint32_t klen)
{
	return db_put(db, key, klen, NULL, 0);
}

int
db_iter(db_t *db, db_iter_t *iter, const void *key, const uint32_t klen)
{
	uint64_t    i;
	uint64_t    hash;
	db_table_t  table;
	db_bucket_t bucket;

	if (key == NULL || klen == 0) {
		iter->table_off  = 0;
		iter->bucket_off = 0;

		return DB_OK;
	}

	hash = db_hash(key, klen);
	db_table_read(db, &table, hash % db->db_table_len);

	for (i = hash % table.bucket_len;; i = (i + 1) % table.bucket_len) {
		uint64_t koff;

		db_bucket_read(db, &table, &bucket, i);

		if (bucket.hash == 0)
			break;

		if (bucket.hash != hash)
			continue;

		if (db_file_compare(db->db_data, &klen, bucket.off,
					sizeof(klen)) != 0)
		{
			continue;
		}
	
		koff = bucket.off + sizeof(klen) + sizeof(uint32_t);
		if (db_file_compare(db->db_data, key, koff, klen) == 0) {
			iter->table_off  = hash % table.bucket_len;
			iter->bucket_off = i;

			return DB_OK;
		}
	}

	return DB_ERROR;
}

int
db_iter_next(db_t *db, db_iter_t *iter,
        void *key, uint32_t *klen, void *val, uint32_t *vlen)
{
	uint64_t i;
	uint64_t j;

	for (i = iter->table_off; i < db->db_table_len; i++) {
		db_table_t table;

		db_table_read(db, &table, i);
		for (j = iter->bucket_off; j < table.bucket_len; j++) {
			uint64_t off;
			uint32_t dbklen;
			uint32_t dbvlen;
			db_bucket_t bucket;

			db_bucket_read(db, &table, &bucket, j);
			if (bucket.hash == 0)
				continue;

			off = bucket.off;
			off += db_file_read(db->db_data, &dbklen, off,
						sizeof(dbklen));
			off += db_file_read(db->db_data, &dbvlen, off,
						sizeof(dbvlen));

			if (dbvlen == 0)
				continue;

			if (dbklen < *klen)
				*klen = dbklen;

			if (dbvlen < *vlen)
				*vlen = dbvlen;

			off += db_file_read(db->db_data, key, off, *klen);
			off += db_file_read(db->db_data, val, off, *vlen);

			*klen = dbklen;
			*vlen = dbvlen;

			iter->bucket_off = j + 1;
			return DB_OK;
		}
		iter->table_off += 1;
		iter->bucket_off = 0;
	}

	return DB_SYS_ERROR;
}

int
db_stat(db_t *db, db_stat_t *stat)
{
	int error;
	uint64_t i;
	uint32_t klen;
	uint32_t vlen;

	db_iter_t iter;

	memset(stat, 0, sizeof(db_stat_t));

	stat->db_file_size = db_file_size(db->db_data);

	stat->db_table_min = UINT32_MAX;
	for (i = 0; i < db->db_table_len; i++) {
		db_table_t table;
		db_table_read(db, &table, i);

		if (table.bucket_key > stat->db_table_max)
			stat->db_table_max = table.bucket_key;

		if (table.bucket_key < stat->db_table_min)
			stat->db_table_min = table.bucket_key;
		stat->db_table_total  += table.bucket_key;
		stat->db_bucket_total += table.bucket_len;
	}
	stat->db_table_size  = stat->db_table_total * sizeof(db_table_t);
	stat->db_bucket_size = stat->db_bucket_total * sizeof(db_bucket_t);

        if ((error = db_iter(db, &iter, NULL, 0)) != DB_OK)
		return error;

	klen = 0;
	vlen = 0;
        while (db_iter_next(db, &iter, NULL, &klen, NULL, &vlen) == DB_OK) {
		stat->db_data_size += klen;
		stat->db_data_size += vlen;
		klen = 0;
		vlen = 0;
        }

	return DB_OK;
}

int
db_close(db_t *db)
{
	return db_file_close(db->db_data);
}
