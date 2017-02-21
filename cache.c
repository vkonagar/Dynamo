
/* Thread-safe cache implementation using LinkedList.
 * *************************************************
 * This cache implementation uses a linkedlist by locking at two levels.
 * All of the locks are reader-writer locks.
 * There is a cache level lock, which is used when a thread wants to
 * read/update the head of the linkedlist of the cache.
 *
 * If threads want to just access an element inside the cache, they acquire
 * the read writer lock of that element.
 *
 * Cache is implemented using LRU with unix timestamps. When a thread accesses
 * a data item, its timestamp is updated to the latest timestamp and when
 * eviction is done, the oldest entry is evicted.
 *
 * This is an approximation of LRU and is not a strict LRU as there will be
 * scenarios in which multiple threads access the entry at the same time.
 *
 * Author: Vamshi Reddy Konagari (vkonagar@andrew.cmu.edu)
 * Date: 12/4/2016
 */
#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "csapp.h"


void display_cache(cache_t* cache)
{
    cache_entry_t* temp = cache->head;
    printf("--------START-----\n");
    while(temp)
    {
        printf("%s:%p:%d\n", temp->data->key.key_data, temp->data->value.value_data,
                    temp->timestamp);
        temp = temp->next;
    }
    printf("--------END------\n");
}

void get_global_cache_wrlock(cache_t* cache)
{
    Pthread_rwlock_wrlock(&cache->lock);
}

void release_global_cache_wrlock(cache_t* cache)
{
    Pthread_rwlock_unlock(&cache->lock);
}

/* get_new_cache_entry
 * Creates a new cache entry by allocating the memory on the heap
 * @return new cache entry's address.
 */
cache_entry_t* get_new_cache_entry()
{
    cache_entry_t* entry = (cache_entry_t*)malloc(sizeof(cache_entry_t));
    entry->data = NULL;
    entry->prev = NULL;
    entry->next = NULL;
    entry->data_size = 0;
    /* Initialize the lock */
    if (pthread_rwlock_init(&entry->lock, NULL) != 0)
    {
        perror("Cannot initialize the cache entry lock\n");
        exit(EXIT_FAILURE);
    }
    return entry;
}


/* get_new_cache
 * Creates a new cache by allocating the memory on the heap
 * @return new cache entry's address.
 */
cache_t* get_new_cache()
{
    printf("Initializing Code Cache with %d bytes\n", MAX_CACHE_SIZE);
    cache_t* cache = (cache_t*) Malloc(sizeof(cache_t));
    cache->total_size = 0;
    cache->head = NULL;
    /* Initialize the cache lock */
    if (pthread_rwlock_init(&cache->lock, NULL) != 0)
    {
        perror("Cannot initialize the cache entry lock\n");
        exit(EXIT_FAILURE);
    }
    return cache;
}

/* free_cache_entry
 * Destroys the mutex and frees a given cache entry
 * @param cache entry to be destroyed.
 */
void free_cache_entry(cache_entry_t* entry)
{
    if (pthread_rwlock_destroy(&entry->lock) != 0)
    {
        perror("Can't destroy the cache entry lock\n");
        exit(EXIT_FAILURE);
    }
    Free(entry->data); // free the dynamically allocated data.
    Free(entry);
}

/* Add a new entry into the cache.
 * @param cache cache to which an entry to be added
 * @param entry entry to be added
 *
 * Inserts the cache entry into the cache linkedlist by adding it to the head.
 * @return errocode.
 * */
int add_to_cache(cache_t* cache, cache_entry_t* entry)
{
    /* Lock the cache */
    Pthread_rwlock_wrlock(&cache->lock);
    /* Keep deleting the old objects until this object fits in the cache */
    while ((cache->total_size + entry->data_size) > MAX_CACHE_SIZE)
    {
        /* If the cache doesn't have any entry and can't fit this item
         * then break */
        if (cache->total_size == 0)
        {
            Pthread_rwlock_unlock(&cache->lock);
            return CACHE_INSERT_ERR;
        }
        delete_lru_entry(cache);
    }

    if (gettimeofday(&entry->timestamp, NULL) == -1)
    {
        perror("gettimeofday");
        free_cache_entry(entry);
    }

    cache_entry_t* head = cache->head;
    if (head == NULL)
    {
        cache->head = entry;
        cache->total_size += entry->data_size;
        Pthread_rwlock_unlock(&cache->lock);
        return CACHE_INSERT_SUCCESS;
    }

    entry->next = head;
    entry->prev = NULL;
    cache->head = entry;
    entry->next->prev = entry;
    cache->total_size += entry->data_size;
    /* Unlock the cache */
    Pthread_rwlock_unlock(&cache->lock);
    return CACHE_INSERT_SUCCESS;
}

/* delete_lru_entry
 * deletes a Least referenced entry from the cache using LRU algorithm.
 * @param cache cache address.
 * ASSUMPTION: cache lock should be taken before calling this function.
 * @return errcode
 * */
int delete_lru_entry(cache_t* cache)
{
    cache_entry_t* temp = cache->head;
    cache_entry_t* lru_entry = NULL;
    struct timeval current_time, lru_ts;

    if (gettimeofday(&current_time, NULL) == -1)
    {
        printf("Error fetching the time\n");
        return CACHE_DELETE_ERR;
    }

    lru_ts = current_time;
    /* Find LRU entry */
    while(temp)
    {
        struct timeval ts_tmp;
        timersub(&current_time, &temp->timestamp, &ts_tmp);
        if (ts_tmp.tv_sec < lru_ts.tv_sec)
        {
            lru_ts = temp->timestamp;
            lru_entry = temp;
        }
        else if (ts_tmp.tv_sec == lru_ts.tv_sec)
        {
            if (ts_tmp.tv_usec < lru_ts.tv_usec)
            {
                lru_ts = temp->timestamp;
                lru_entry = temp;
            }
        }
        temp = temp->next;
    }

    if (lru_entry == NULL)
    {
        printf("Error deleting entry from the cache\n");
        return CACHE_DELETE_ERR;
    }

    if (lru_entry->prev == NULL && lru_entry->next == NULL)
    {
        cache->head = NULL;
    }
    else if (lru_entry->prev == NULL)
    {
        cache->head = lru_entry->next;
        lru_entry->next->prev = NULL;
    }
    else if (lru_entry->next == NULL)
    {
        lru_entry->prev->next = NULL;
    }
    else
    {
        lru_entry->prev->next = lru_entry->next;
        lru_entry->next->prev = lru_entry->prev;
    }
    printf("Evicted %s \n", lru_entry->data->key.key_data);
    /* Call back is called to perform clean up */
    if (lru_entry->delete_callback != NULL)
        lru_entry->delete_callback(lru_entry->data);
    cache->total_size -= lru_entry->data_size;
    free_cache_entry(lru_entry);
    return CACHE_DELETE_SUCCESS;
}


/* Gets the cached data for the given host_name, port, url
 * updates the cached_data_len to the length of the data buffer returned.
 * @return pointer to the data buffer
 */
cache_data_item_t* get_cached_data(cache_t* cache, cache_key_t* key)
{
    /* Take read lock on the cache. Multiple readers can take the data
     * from the cache. */
    Pthread_rwlock_rdlock(&cache->lock);
    cache_entry_t* temp = cache->head;
    cache_data_item_t* result = NULL;
    while(temp)
    {
        if ((strcmp(temp->data->key.key_data, key->key_data) == 0))
        {
            /* If entry is found, grab a write lock to update the timestamp */
            Pthread_rwlock_wrlock(&temp->lock);
            /* Copy the data to a new buffer and return it to the client */
            if (gettimeofday(&temp->timestamp, NULL) == -1)
            {
                Pthread_rwlock_unlock(&temp->lock);
                Pthread_rwlock_unlock(&cache->lock);
                printf("Cannot get the time \n");
                return NULL;
            }
            result = temp->data;
            /* Unlock the write lock */
            Pthread_rwlock_unlock(&temp->lock);
            break;
        }
        temp = temp->next;
    }
    Pthread_rwlock_unlock(&cache->lock); /* Unlock read lock on cache */
    return result;
}
