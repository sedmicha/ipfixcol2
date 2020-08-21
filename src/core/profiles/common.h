/**
 * \file src/core/profiles/common.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz> 
 * \brief Profiles common
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#ifndef IPX_PROFILES_COMMON_H
#define IPX_PROFILES_COMMON_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h> //???
#include "../verbose.h"

#define PROFILES_ERROR(fmt, ...)     IPX_ERROR("profiles", fmt, ## __VA_ARGS__)
#define PROFILES_MEMORY_ERROR()      PROFILES_ERROR("cannot allocate memory at %s:%d", __FILE__, __LINE__)

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
