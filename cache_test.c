#include<stdio.h>
#include"cache.h"

int main()
{
    cache_t* cache = get_new_cache();
    int i;
    for(i=0;i<1000;i++)
    {
        cache_entry_t* entry = get_new_cache_entry();
        entry->data = malloc(sizeof(cache_data_item_t));
        sprintf(entry->data->key.key_data, "%s%d", "vam", i);
        entry->data->value.value_data = i;
        entry->data_size = 20;

        if (add_to_cache(cache, entry) == CACHE_INSERT_ERR)
        {
            printf("Cannot insert\n");
        }
        printf("Inserted vam%d\n", i);
    }
    cache_key_t key = {"vam999"};
    cache_data_item_t* item = get_cached_data(cache, &key);
    display_cache(cache);
    if (item != NULL)
    {
        printf("value is %d\n", (int)item->value.value_data);
    }
    display_cache(cache);
}
