#ifndef IPX_PROFILES_COMMON_H
#define IPX_PROFILES_COMMON_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h> //???
#include "../verbose.h"

#define PROFILES_ERROR(fmt, ...)     IPX_ERROR("profiles", fmt, ## __VA_ARGS__)
#define PROFILES_MEMORY_ERROR()      PROFILES_ERROR("cannot allocate memory")

/**
 * Add an element to the end of array
 * 
 * \param[in,out] items          Pointer to the pointer of the first item of the array
 * \param[in,out] items_cnt_ptr  Pointer to the number of items in the array
 * \param[in]     item_size      The size of one item
 * 
 * \return pointer to the added element, or NULL on failure 
 */
void *
array_push(void **items, size_t *items_cnt_ptr, size_t item_size);

static inline void
set_bit(uint64_t *bitset, int idx)
{
    bitset[idx >> 6] |= 1 << (idx & 0b111111);
}

static inline void
clear_bit(uint64_t *bitset, int idx)
{
    bitset[idx >> 6] &= ~(1 << (idx & 0b111111)); 
}

static inline bool
test_bit(uint64_t *bitset, int idx)
{
    return bitset[idx >> 6] & (1 << (idx & 0b111111));
}

#endif // IPX_PROFILES_COMMON_H