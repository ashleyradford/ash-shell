#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "elist.h"
#include "logger.h"

#define DEFAULT_INIT_SZ 10
#define RESIZE_MULTIPLIER 2

struct elist {
    size_t capacity;         /*!< Storage space allocated for list items */
    size_t size;             /*!< The actual number of items in the list */
    void **element_storage;  /*!< Pointer to the beginning of the array  */
};                                  /* An array of void stars, pointer to the first void star */

bool idx_is_valid(struct elist *list, size_t idx);

struct elist *elist_create(size_t list_sz)
{
    struct elist *list = malloc(sizeof(struct elist));
    if (list == NULL) {
        return NULL;
    }

    if (list_sz == 0) {
        list_sz = DEFAULT_INIT_SZ;
    }

    list->size = 0;
    list->capacity = list_sz;
    list->element_storage = malloc(sizeof(void *) * list->capacity);
    if (list->element_storage == NULL) {
        free(list);
        return NULL;
    }

    LOG("Creating new elist, size = %zu, capacity = %zu, start address = %p\n",
        list->size,
        list->capacity,
        list->element_storage);

    return list;
}

void elist_destroy(struct elist *list)
{
    free(list->element_storage);
    free(list);
}

int elist_set_capacity(struct elist *list, size_t capacity)
{
    if (capacity == 0) {
        /* Set the element storage to NULL, set the new capacity,
         * return, and ensure it gets reallocated + add a condition that checks
         * for capacity == 0, and realloc to default capacity */
        list->element_storage = NULL;
        list->capacity = 0;
        list->size = 0;
        return 0;
    }

    if (capacity == list->capacity) {
        return 0;
    }

    LOG("Setting new list capacity: %zu (old capacity = %zu)\n", capacity, list->capacity);
    void *new_elements = realloc(list->element_storage, sizeof(void *) * capacity);
    if (new_elements == NULL) {
        return -1;
    }

    list->element_storage = new_elements;
    list->capacity = capacity;

    /* Making list smaller */
    if (list->capacity < list->size) {
        list->size = list->capacity;
    }

    return 0;
}

size_t elist_capacity(struct elist *list)
{
    return list->capacity;
}

ssize_t elist_add(struct elist *list, void *item)
{
    if (list->size >= list->capacity) {
        if (list->capacity == 0) {
            list->capacity = DEFAULT_INIT_SZ;
        }
        if (elist_set_capacity(list, list->capacity * RESIZE_MULTIPLIER) == -1) {
            return -1;
        }
    }

    if (list->element_storage == NULL) {
        list->element_storage = malloc(sizeof(void *) * list->capacity);
        if (list->element_storage == NULL) {
            return -1;
        }
    }

    size_t idx = list->size++;
    list->element_storage[idx] = item;
    return idx;
}

int elist_set(struct elist *list, size_t idx, void *item)
{
    if (!idx_is_valid(list, idx)) {
       return -1; 
    }
    list->element_storage[idx] = item;
    return 0;
}

void *elist_get(struct elist *list, size_t idx)
{
    if (!idx_is_valid(list, idx)) {
        return NULL;
    }
    return list->element_storage[idx];
}

void **elist_elements(struct elist *list) {
    return list->element_storage;
}

size_t elist_size(struct elist *list)
{
    return list->size;
}

int elist_remove(struct elist *list, size_t idx)
{
    if (!idx_is_valid(list, idx)) {
       return -1; 
    }
    
    memmove(list->element_storage+idx, list->element_storage+(idx+1), sizeof(void *) * (list->size - idx - 1));
    LOG("Size of memmove: %zu\n", sizeof(void *) * (list->size - idx - 1));
    list->element_storage[list->size-1] = NULL;
    list->size--;
    return 0;
}

void elist_clear(struct elist *list)
{
    /* Resizing to 0 */
    list->element_storage = NULL;
    list->size = 0;
}

void elist_clear_mem(struct elist *list)
{
    /* Zeroing out the elements and resizing to 0 */
    for (int i = 0; i < list->size; i++) {
        elist_set(list, i, NULL);
    }

    list->element_storage = NULL;
    list->size = 0;
}

ssize_t elist_index_of(struct elist *list, void *item, size_t item_sz)
{
    for (ssize_t i = 0; i < list->size; i++) {
        if (memcmp(list->element_storage[i], item, item_sz) == 0) {
            return i;
        }
    }
    return -1;
}

void elist_sort(struct elist *list, int (*comparator)(const void *, const void *))
{
    qsort(list->element_storage, list->size, sizeof(list->element_storage[0]), comparator);
}

bool idx_is_valid(struct elist *list, size_t idx)
{
    return !(idx >= list->size);
}
