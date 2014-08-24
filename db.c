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

#define DB_MAGIC	0x424400
#define DB_VERSION	1

#define TABLE_LEN	(256)
#define TABLE_SIZE	(TABLE_LEN * sizeof(struct table))

#define SLOT_LEN	(TABLE_LEN * 256)
#define SLOT_SIZE	(SLOT_LEN * sizeof(struct slot))

#define DB_SIZE		(DB_HEADER_SIZE + TABLE_SIZE + SLOT_SIZE + 65535)

#define PAGE_ALIGN(ptr,pgsize)	\
	((char *)(ptr) - (((char *)(ptr) - (char *)NULL) & ((pgsize) - 1)))

static int
db_mmap(db_t *db);

static int
db_sync(db_t *db, off_t offset, size_t size);

static int
db_likely(db_t *db, off_t off, size_t len);

static int
db_unlikely(db_t *db, off_t off, size_t len);

static size_t
db_file_size(db_t *db);

static int
db_file_resize(db_t *db, size_t size);

static void
db_init(db_t *db);

static uint64_t
db_alloc(db_t *db, uint64_t len);

static int
db_read(db_t *db, void *buf, off_t off, size_t len);

static int
db_write(db_t *db, const void *buf, off_t off, size_t len);

static int
db_cmp(db_t *db, const void *buf, off_t off, size_t len);

static uint64_t
db_slot_find(db_t *db, uint64_t ptr, uint64_t slot_len,
	uint64_t hash, const void *key, uint32_t klen);

int
db_open(db_t *db, const char *filename, int mode)
{
	int    init = 0;
	size_t size = 0;

	memset(db, 0, sizeof(struct db));

	db->db_file = open(filename, O_RDWR | O_CREAT, 0644);

	size = db_file_size(db);

	if (size == 0) {
		db_file_resize(db, DB_SIZE);
	} else {
		init = 1;
	}

	if (db_mmap(db) != DB_OK)
		return DB_ERR;

	if (!init)
		db_init(db);
	else if (db->db_magic != DB_MAGIC || db->db_version != DB_VERSION)
		return DB_ERR;

	db->db_pgsize = sysconf(_SC_PAGESIZE);

	db_likely(db, 0, DB_HEADER_SIZE + TABLE_LEN);

	return DB_OK;
}

static void
db_init(db_t *db)
{
	int i;
	uint64_t slot_ptr = DB_HEADER_SIZE + TABLE_SIZE;

	assert(db && db->db_data);

        db->db_magic		= DB_MAGIC;
        db->db_version		= DB_VERSION;
        db->db_size		= db->db_data_size;
	db->db_slot_ptr		= slot_ptr;
	db->db_slot_len 	= SLOT_LEN;

	for (i = 0; i < TABLE_LEN; i++) {
		uint64_t len = db->db_slot_len / TABLE_LEN;
		uint64_t ptr = slot_ptr + i * sizeof(struct slot) * len;

		db->tables[i].slot_ptr = ptr;
		db->tables[i].slot_key = 0;
		db->tables[i].slot_len = len;
	}

	db->db_free_ptr = slot_ptr + db->db_slot_len * sizeof(struct slot);
}

static size_t
db_file_size(db_t *db)
{
	struct stat stat;

	if (fstat(db->db_file, &stat) < 0) {
		perror("fstat");
		return 0;
	}
	return stat.st_size;
}

static int
db_file_resize(db_t *db, size_t size)
{
	assert(db->db_file);

	if (ftruncate(db->db_file, size) < 0) {
		perror("ftruncate");
		return DB_ERR;
	}
	return DB_OK;
}

int
db_read(db_t *db, void *buf, off_t off, size_t len)
{
	assert(off + len < db->db_size);

	memcpy(buf, (uint8_t *)db->db_data + off, len);
	return len;
}

int
db_write(db_t *db, const void *buf, off_t off, size_t len)
{
	assert(off + len < db->db_size);

	memcpy((uint8_t *)db->db_data + off, buf, len);
	return len;
}

int
db_cmp(db_t *db, const void *buf, off_t off, size_t len)
{
	assert(off + len < db->db_size);

	return memcmp((uint8_t *)db->db_data + off, buf, len);
}

/*
static void
db_copy(db_t *db, off_t dst, off_t src, size_t len)
{
	memcpy((uint8_t *)db->db_data + dst, (uint8_t *)db->db_data + src, len);
}
*/

static int
db_mmap(db_t *db)
{
	assert(db && db->db_file);

	if (db->db_data != NULL) {
		if (db_sync(db, 0, db->db_data_size) != DB_OK) {
			return DB_ERR;
		}
		if (munmap(db->db_data, db->db_data_size) < 0) {
			perror("munmap");
			return DB_ERR;
		}
	}

	db->db_data_size = db_file_size(db);
	db->db_data      = mmap(NULL,
				db->db_data_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, db->db_file, 0);
	if (db->db_data == MAP_FAILED) {
		perror("mmap");
		return DB_ERR;
	}
	db->header = db->db_data;
	db->tables = (struct table *)((uint8_t *)db->db_data + DB_HEADER_SIZE);

	return DB_OK;
}

static int 
db_sync(db_t *db, off_t off, size_t len)
{
	void *ptr = (uint8_t *)db->db_data + off;
	if (msync(PAGE_ALIGN(ptr, db->db_pgsize), len, MS_SYNC) < 0) {
		perror("msync");
		return DB_ERR;
	}
	return DB_OK;
}

static int
db_likely(db_t *db, off_t off, size_t len)
{
	void *ptr = (uint8_t *)db->db_data + off;
	if (madvise(PAGE_ALIGN(ptr, db->db_pgsize), len, MADV_WILLNEED) < 0) {
		perror("madvise");
		return DB_ERR;
	}
	return DB_OK;
}

static int
db_unlikely(db_t *db, off_t off, size_t len)
{
	void *ptr = (uint8_t *)db->db_data + off;
	if (madvise(PAGE_ALIGN(ptr, db->db_pgsize), len, MADV_DONTNEED) < 0) {
		perror("madvise");
		return DB_ERR;
	}
	return DB_OK;
}

uint64_t
db_alloc(db_t *db, uint64_t len)
{
	uint64_t ptr;
	if ((db->db_free_ptr + len) > db->db_size) {
		size_t newsize = (db->db_free_ptr + len) * 2;

		if (db_file_resize(db, newsize) != DB_OK) {
			perror("resize");
			return 0;
		}
		if (db_mmap(db) != DB_OK) {
			perror("mmap");
			return 0;
		}

		db->db_size = newsize;
	}

	ptr = db->db_free_ptr;
	db->db_free_ptr += len;
	return ptr;
}

uint64_t
db_calloc(db_t *db, uint64_t len)
{
	uint64_t ptr = db_alloc(db, len);
	if (ptr == 0)
		return 0;

	memset((uint8_t *)db->db_data + ptr, 0, len);

	return ptr;
}

int
db_rehash(db_t *db, uint32_t table)
{
	int i;
	uint64_t slot_ptr;
	uint64_t slot_len;
	uint64_t slot_new_ptr;
	uint64_t slot_new_len;

	int rehashed = 1;

	/* alloc new slot if not did before */
	if (db->db_slot_new_ptr == 0) {
		slot_new_len = db->db_slot_len * 2;
		slot_new_ptr = db_calloc(db, slot_new_len * sizeof(struct slot));
		if (slot_new_ptr == 0)
			return DB_ERR;

		db->db_slot_new_ptr = slot_new_ptr;
		db->db_slot_new_len = slot_new_len;

		db_likely(db, slot_new_ptr, slot_new_len);
	}

	slot_ptr = db->tables[table].slot_ptr;
	slot_len = db->tables[table].slot_len;

	slot_new_len = slot_len * 2;
	slot_new_ptr = db->db_slot_new_ptr + table * slot_new_len * sizeof(struct slot);
 
	/* read old slot from table and insert to new table */
	for (i = 0; i < slot_len; i++) {
		uint64_t ptr;
		struct slot slot;

		db_read(db, &slot, slot_ptr + i * sizeof(struct slot), sizeof(struct slot));
		if (slot.hash == 0)
			continue;
		ptr = db_slot_find(db, slot_new_ptr, slot_new_len, slot.hash, NULL, 0);

		/*
		 * this fault is a table when rehash and need rehash again
		 * change another hash algorithm
		 */
		if (ptr == 0)
			return DB_ERR;

		db_write(db, &slot, ptr, sizeof(struct slot));
	}

	db->tables[table].slot_ptr = slot_new_ptr;
	db->tables[table].slot_len = slot_new_len;

	db_sync(db, DB_HEADER_SIZE, TABLE_SIZE);
	db_sync(db, slot_new_ptr, slot_new_len * sizeof(struct slot));

        for (i = 0; i < TABLE_LEN; i++) {
		struct table *table = &db->tables[i];

		if (table->slot_ptr < db->db_slot_new_ptr) {
			rehashed = 0;
			break;
		}
        }
        if (rehashed) {
		db_unlikely(db, db->db_slot_ptr, db->db_slot_len * sizeof(struct slot));
		db->db_slot_ptr = db->db_slot_new_ptr;
		db->db_slot_len = db->db_slot_new_len;
		db->db_slot_new_ptr = 0;
		db->db_slot_new_len = 0;
        }

	return DB_OK;
}

/*
 * will find the right slot for key
 */
static uint64_t
db_slot_find(db_t *db, uint64_t ptr, uint64_t slot_len, uint64_t hash, const void *key, uint32_t klen)
{
	uint64_t i;
	uint64_t j;
	struct slot slot;

	for (i = 0, j = hash; i < slot_len; i++, j++) {
		j = j % slot_len;

		db_read(db, &slot, ptr + j * sizeof(struct slot), sizeof(struct slot));

		if (slot.hash == 0)
			return ptr + j * sizeof(struct slot);

		if (klen == 0)
			continue;

		if (slot.hash != hash)
			continue;

		/* klen not equal */
		if (db_cmp(db, &klen, slot.ptr, sizeof(uint32_t)) != 0)
			continue;

		/* key not equal */
		if (db_cmp(db, key, slot.ptr + sizeof(uint32_t) * 2, klen) == 0)
			return ptr + j * sizeof(struct slot);
	}
	return 0;
}

int
db_put(db_t *db, const void *key, uint32_t klen, const void *val, uint32_t vlen)
{
	uint32_t len;
	uint64_t ptr;
	uint64_t data;
	struct slot slot;

	const uint64_t hash  = db_hash(key, klen);
	struct table  *table = &db->tables[hash & 0xFF];

	len  = sizeof(uint32_t) * 2 + klen + vlen;
	data = db_alloc(db, len);

	data += db_write(db, &klen, data, sizeof(uint32_t));
	data += db_write(db, &vlen, data, sizeof(uint32_t));                       
	data += db_write(db, key, data, klen);
	data += db_write(db, val, data, vlen);

	slot.hash = hash;
	slot.ptr  = data - len;

	ptr = db_slot_find(db, table->slot_ptr, table->slot_len, hash, key, klen);
	if (ptr == 0)
		return DB_ERR;

	/* write slot */
	db_write(db, &slot, ptr, sizeof(struct slot));

	table->slot_key = table->slot_key + 1;
	if ((table->slot_key * 2) > table->slot_len) {
		if (db_rehash(db, hash & 0xFF) != DB_OK)
			return DB_ERR;
	}

	return DB_OK;
}

uint32_t
db_get(db_t *db, const void *key, uint32_t klen, void *val, uint32_t vlen)
{
	uint32_t len;
	uint64_t ptr;
	uint64_t data;

	const uint64_t      hash  = db_hash(key, klen);
	const struct table *table = &db->tables[hash & 0xFF];

	ptr = db_slot_find(db, table->slot_ptr, table->slot_len, hash, key, klen);
	if (ptr == 0)
		return 0;

	if (db_cmp(db, &hash, ptr, sizeof(hash)) != 0)
		return 0;

	db_read(db, &data, ptr + sizeof(hash), sizeof(data));

	data += sizeof(uint32_t);	/* skip klen	*/
	data += db_read(db, &len, data, sizeof(uint32_t));
	data += klen;			/* skip key	*/

	vlen = (len < vlen) ? len : vlen;
	db_read(db, val, data, vlen);

	return vlen;
}

int
db_close(db_t *db)
{
	msync(db->db_data, db->db_data_size, MS_SYNC);
	munmap(db->db_data, db->db_data_size);
	close(db->db_file);
	return DB_OK;
}

