#ifndef _DLLIST_H_INCLUDED_
#define _DLLIST_H_INCLUDED_

struct dll_entry {
    struct dll_entry *prev;
    struct dll_entry *next;
    unsigned char item[0];
};

struct dllist {
    struct dll_entry *head;
    struct dll_entry *tail;
    int size;
};

#define dllist_add(list, item) \
    __dllist_add(list, item, sizeof(*(item)))
#define dllist_entry(item) \
    (struct dll_entry *)((char *)item - sizeof(struct dll_entry))

#define dllist_foreach(var, list) \
    for (struct dll_entry *_dle = (list)->head; _dle && (var = (typeof(var))_dle->item); _dle = _dle->next)

extern struct dllist *dllist_create();
extern void *__dllist_add(struct dllist *list, void *item, size_t size);
extern void dllist_delete(struct dllist *list, void *item);
extern void dllist_clear(struct dllist *list);

#endif
