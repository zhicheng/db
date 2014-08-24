#ifndef __HASH_H__
#define __HASH_H__

#include <stdint.h>
#include <stdlib.h>

uint64_t
db_hash(const void *key, size_t len);

#endif /* __HASH_H__ */

