/*
 * Header file for cache implementation.
 * Author: Vamshi Reddy Konagari (vkonagar@andrew.cmu.edu)
 * Date: 12/4/2016
 */
#ifndef PROXY_CACHE_H
#define PROXY_CACHE_H

#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include "util.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 100

#define CACHE_INSERT_ERR -3
#define CACHE_DELETE_ERR -4
#define CACHE_INSERT_SUCCESS 0
#define CACHE_DELETE_SUCCESS 0


typedef struct cache_key
{
    char key_data[MAX_DLL_NAME_LENGTH]; /* Name of the library file */
}cache_key_t;

typedef struct cache_value
{
    void* value_data;
}cache_value_t;

/* This represents a .so library module currently loaded in the process's
 * address space */
typedef struct cache_data_item
{
    cache_key_t key;
    cache_value_t value;
}cache_data_item_t;

/* structure of a cache entry. Read write lock is used to protect this. */
typedef struct cache_entry
{
    cache_data_item_t* data;
    pthread_rwlock_t lock;
    int data_size;
    void (*delete_callback)(cache_data_item_t*);
    struct timeval timestamp;
    struct cache_entry* next;
    struct cache_entry* prev;
}cache_entry_t;

typedef struct cache
{
    pthread_rwlock_t lock;
    long total_size; /* total size of the cache in bytes */
    cache_entry_t* head;
}cache_t;

/* Create cache structures */
cache_t* get_new_cache();
cache_entry_t* get_new_cache_entry();


int add_to_cache(cache_t* cache, cache_entry_t* entry);
int delete_lru_entry(cache_t* cache);
void free_cache_entry(cache_entry_t* entry);

cache_data_item_t* get_cached_data(cache_t* cache, cache_key_t* key);
void display_cache();
#endif /* End of header */
