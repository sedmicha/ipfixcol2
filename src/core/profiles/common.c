#include "common.h"

void *
array_push(void **items, size_t *items_cnt_ptr, size_t item_size)
{
    void *tmp = realloc(*items, item_size * ((*items_cnt_ptr) + 1));
    if (tmp == NULL) {
        return NULL;
    }
    *items = tmp;
    *items_cnt_ptr += 1;
    void *last_item = (char *)(*items) + (*items_cnt_ptr - 1) * item_size;
    return last_item;
}