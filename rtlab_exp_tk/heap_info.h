#ifndef HEAP_INFO_H
#define HEAP_INFO_H
/*** heap_info.h - Defines the universal heap_info_t data structure which each
 *** heap can provide, and algorithms can use.  An algorithm which uses the
 *** universal definition of a heap data structure can use different heaps
 *** interchangeably for testing or comparison purposes.
 ***/
/*
 *  Shane Saunders
 */

/* all heaps in Calin's adapdation need 64-bit keys */
typedef unsigned long long heap_key_t;

/* Structure to be provided by heaps and used by algorithms. */


typedef struct heap_info {
    int (*delete_min)(void *heap);
    int (*insert)(void *heap, int node, heap_key_t key);
    void (*decrease_key)(void *heap, int node, heap_key_t newkey);
    int (*n)(void *heap);
    long (*key_comps)(void *heap);
    void *(*alloc)(int n_items);
    void (*free)(void *heap);
    void (*dump)(void *heap);
} heap_info_t;

#endif
