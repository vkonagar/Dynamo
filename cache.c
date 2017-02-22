
/* Thread-safe cache implementation using LinkedList.
 * *************************************************
 * This cache implementation uses a linkedlist by locking at two levels.
 * There is a cache level lock, which is used when a thread wants to
 * read/update the head of the linkedlist of the cache.
 *
 * If a thread wants to just access an element inside the cache, it should
 * acquire the read writer lock of that element.
 *
 * Cache is implemented using LRU with unix timestamps. When a thread accesses
 * a data item, the timestamp is updated to the latest timestamp and when
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
        printf("%s:%p\n", temp->data->key.key_data, temp->data->value.value_data);
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
    printf("Cache with maximum %d bytes\n", MAX_CACHE_SIZE);
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
    /* Lock the entire cache. We don't want anyone to be reading while we
     * change the state of the cache pointers */
    Pthread_rwlock_wrlock(&cache->lock);
    /* Keep deleting old objects until this object fits in the cache */
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

    /* Update the latest timestamp on the added entry */
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

    /* Wait if someone else is using this entry. Possiblity that some thread
     * might cache this entry and be using. We can't delete until it releases
     * its read lock */
    Pthread_rwlock_wrlock(&lru_entry->lock);
    /* We don't need to acquire prev and next nodes's write locks as we
     * are assuming that traversals can only happen when the global cache is
     * read locked and it won't happen while this is executing */

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
    dbg_printf("Evicted %s \n", lru_entry->data->key.key_data);
    /* Call back is called to perform clean up */
    if (lru_entry->delete_callback != NULL)
        lru_entry->delete_callback(lru_entry->data);
    cache->total_size -= lru_entry->data_size;
    free_cache_entry(lru_entry);
    return CACHE_DELETE_SUCCESS;
}


/* Gets the cached data for the given key. It also read locks the returned item.
 * @return cached entry
 */
cache_entry_t* get_cached_item_with_lock(cache_t* cache, cache_key_t* key)
{
    /* Take read lock on the cache. Multiple readers can take the data
     * from the cache. */
    Pthread_rwlock_rdlock(&cache->lock);
    cache_entry_t* temp = cache->head;
    cache_entry_t* result = NULL;
    while(temp)
    {
        if ((strcmp(temp->data->key.key_data, key->key_data) == 0))
        {
            /** Critical Section for updating the timestamp */
            Pthread_rwlock_wrlock(&temp->lock);
            if (gettimeofday(&temp->timestamp, NULL) == -1)
            {
                perror("gettimeofdat in get");
                Pthread_rwlock_unlock(&temp->lock);
                Pthread_rwlock_unlock(&cache->lock);
                return NULL;
            }
            Pthread_rwlock_unlock(&temp->lock);
            /** End of CS */

            /* Fetch read lock for the caller to use this item */
            Pthread_rwlock_rdlock(&temp->lock);
            result = temp;
            break;
        }
        temp = temp->next;
    }
    Pthread_rwlock_unlock(&cache->lock); /* Unlock read lock on cache */
    return result;
}
