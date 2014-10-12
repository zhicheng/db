#ifndef __DB_HASH_H__
#define __DB_HASH_H__

#include <stdint.h>
#include <stdlib.h>

uint64_t
db_hash(const void *key, size_t len);

#endif /* __DB_HASH_H__ */

