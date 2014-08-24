#ifndef __DB_H__
#define __DB_H__

#include <stdint.h>
#include <stdlib.h>

enum {DB_ERR = -1, DB_OK = 0};

struct table {
        uint64_t slot_ptr;	/* in disk              */
        uint64_t slot_key;	/* key in use           */
        uint64_t slot_len;	/* slots in table       */
};

struct slot {
        uint64_t hash;	/* key hash     */
        uint64_t ptr;	/* data ptr     */
};

typedef struct db {

	int	 db_file;
        int      db_mode;
	int	 db_pgsize;
	void	*db_data;
        size_t   db_data_size;

	struct  table *tables;

	/* in disk */
	struct {
		uint32_t magic;
		uint32_t version;
		uint64_t size;
		uint64_t slot_ptr;		/* slots addr		*/
		uint64_t slot_len;		/* slots number		*/
		uint64_t slot_new_ptr;		/* rehash slots		*/
		uint64_t slot_new_len;
		uint64_t free_ptr;		/* free space 		*/
	} *header;

#define DB_HEADER_SIZE	(sizeof(*((struct db *)0)->header))

#define db_magic	header->magic
#define db_version	header->version
#define db_size		header->size
#define db_slot_ptr	header->slot_ptr
#define db_slot_len	header->slot_len
#define db_slot_new_ptr	header->slot_new_ptr
#define db_slot_new_len header->slot_new_len
#define db_free_ptr	header->free_ptr

} db_t;

int
db_open(db_t *db, const char *filename, int mode);

int
db_put(db_t *db, const void *key, uint32_t klen, const void *val, uint32_t vlen);

uint32_t
db_get(db_t *db, const void *key, uint32_t klen, void *val, uint32_t vlen);

void
db_stat(db_t *db);

int
db_close(db_t *db);


#endif /* __DB_H__ */

