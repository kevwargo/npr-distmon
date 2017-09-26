#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dllist.h"
#include "debug.h"


struct dllist *dllist_create()
{
    struct dllist *list = (struct dllist *)malloc(sizeof(struct dllist));
    if (! list) {
        DEBUG_PERROR("create dllist");
        exit(1);
    }
    memset(list, 0, sizeof(struct dllist));
    return list;
}

void *__dllist_add(struct dllist *list, void *item, size_t size)
{
    struct dll_entry *new = malloc(size + sizeof(struct dll_entry));
    if (item) {
        memcpy((void *)new->item, item, size);
    }
    new->next = NULL;
    new->prev = list->tail;
    if (list->size == 0) {
        list->head = list->tail = new;
    } else {
        list->tail->next = new;
        list->tail = new;
    }
    list->size++;
    return (void *)new->item;
}

void dllist_delete(struct dllist *list, void *item)
{
    struct dll_entry *entry = dllist_entry(item);
    if (list->head == entry) {
        list->head = entry->next;
    }
    if (list->tail == entry) {
        list->tail = entry->prev;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    list->size--;
    free(entry);
}

void dllist_clear(struct dllist *list)
{
    while (list->head) {
        dllist_delete(list, (void *)list->head->item);
    }
}
