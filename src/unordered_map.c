#include "unordered_map.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct unordered_map_entry {
    void*                       key;
    void*                       value;
    struct unordered_map_entry* chain_next;
    struct unordered_map_entry* prev;
    struct unordered_map_entry* next;
} unordered_map_entry;

struct unordered_map {
    unordered_map_entry** table;
    unordered_map_entry*  head;
    unordered_map_entry*  tail;
    size_t              (*hash_function)(void*);
    bool                (*equals_function)(void*, void*);
    size_t                mod_count;
    size_t                table_capacity;
    size_t                size;
    size_t                max_allowed_size;
    size_t                mask;
    float                 load_factor;
};

struct unordered_map_iterator {
    unordered_map*       map;
    unordered_map_entry* next_entry;
    size_t               iterated_count;
    size_t               expected_mod_count;
};

static unordered_map_entry* unordered_map_entry_alloc(void* key, void* value)
{
    unordered_map_entry* entry = malloc(sizeof(*entry));

    if (!entry) 
    {
        return NULL;
    }
    
    entry->key        = key;
    entry->value      = value;
    entry->chain_next = NULL;
    entry->next       = NULL;
    entry->prev       = NULL;

    return entry;
}

static const float  MINIMUM_LOAD_FACTOR = 0.2f;
static const size_t MINIMUM_INITIAL_CAPACITY = 16;

static float maxf(float a, float b) 
{
    return a < b ? b : a;
}

static int maxi(int a, int b) 
{
    return a < b ? b : a;
}

/*******************************************************************************
* Makes sure that the load factor is no less than a minimum threshold.         *
*******************************************************************************/  
static float fix_load_factor(float load_factor)
{
    return maxf(load_factor, MINIMUM_LOAD_FACTOR);
}

/*******************************************************************************
* Makes sure that the initial capacity is no less than a minimum allowed and   *
* is a power of two.                                                           * 
*******************************************************************************/  
static size_t fix_initial_capacity(size_t initial_capacity) 
{
    size_t ret;

    initial_capacity = maxi(initial_capacity, MINIMUM_INITIAL_CAPACITY);
    ret = 1;

    while (ret < initial_capacity) 
    {
        ret <<= 1;
    }
        
    return ret;
}

unordered_map* unordered_map_alloc(size_t initial_capacity,
                                   float load_factor,
                                   size_t (*hash_function)(void*),
                                   bool (*equals_function)(void*, void*))
{
    unordered_map* map;

    if (!hash_function || !equals_function)  
    {
        return NULL;
    }

    map = malloc(sizeof(*map));

    if (!map) 
    {
        return NULL;
    }
    
    load_factor      = fix_load_factor(load_factor);
    initial_capacity = fix_initial_capacity(initial_capacity);

    map->load_factor      = load_factor;
    map->table_capacity   = initial_capacity;
    map->size             = 0;
    map->mod_count        = 0;
    map->head             = NULL;
    map->tail             = NULL;
    map->table            = calloc(initial_capacity, 
                                   sizeof(unordered_map_entry*));
    map->hash_function    = hash_function;
    map->equals_function  = equals_function;
    map->mask             = initial_capacity - 1;
    map->max_allowed_size = (size_t)(initial_capacity * load_factor);

    return map;
}

static void ensure_capacity(unordered_map* map) 
{
    size_t new_capacity;
    size_t new_mask;
    size_t index;
    unordered_map_entry* entry;
    unordered_map_entry** new_table;

    if (map->size < map->max_allowed_size) 
    {
        return;
    }
    
    new_capacity = 2 * map->table_capacity;
    new_mask     = new_capacity - 1;
    new_table    = calloc(new_capacity, sizeof(unordered_map_entry*));
    
    if (!new_table)
    {
        return;
    }
    
    /* Rehash the entries. */
    for (entry = map->head; entry; entry = entry->next)
    {
        index = map->hash_function(entry->key) & new_mask;
        entry->chain_next = new_table[index];
        new_table[index] = entry;
    }

    free(map->table);
    
    map->table            = new_table;
    map->table_capacity   = new_capacity;
    map->mask             = new_mask;
    map->max_allowed_size = (size_t)(new_capacity * map->load_factor);
}

void* unordered_map_put(unordered_map* map, void* key, void* value)
{
    size_t index;
    size_t hash_value;
    void* old_value;
    unordered_map_entry* entry;

    if (!map) 
    {
        return NULL;
    }
    
    hash_value = map->hash_function(key);
    index = hash_value & map->mask;

    for (entry = map->table[index]; entry; entry = entry->chain_next)
    {
        if (map->equals_function(entry->key, key))
        {
            old_value = entry->value;
            entry->value = value;
            return old_value;
        }
    }

    ensure_capacity(map);

    /* Recompute the index since it is possibly changed by 'ensure_capacity' */
    index             = hash_value & map->mask;
    entry             = unordered_map_entry_alloc(key, value);
    entry->chain_next = map->table[index];
    map->table[index] = entry;

    /* Link the new entry to the tail of the list. */
    if (!map->tail)
    {
        map->head = entry;
        map->tail = entry;
    }
    else
    {
        map->tail->next = entry;
        entry->prev = map->tail;
        map->tail = entry;
    }

    map->size++;
    map->mod_count++;
    
    return NULL;
}

bool unordered_map_contains_key(unordered_map* map, void* key)
{
    size_t index;
    unordered_map_entry* entry;

    if (!map) 
    {
        return false;
    }
    
    index = map->hash_function(key) & map->mask;

    for (entry = map->table[index]; entry; entry = entry->chain_next) 
    {
        if (map->equals_function(key, entry->key))
        {
            return true;
        }
    }

    return false;
}

void* unordered_map_get(unordered_map* map, void* key)
{
    size_t index;
    unordered_map_entry* p_entry;

    if (!map) 
    {
        return NULL;
    }
    
    index = map->hash_function(key) & map->mask;

    for (p_entry = map->table[index]; p_entry; p_entry = p_entry->chain_next)
    {
        if (map->equals_function(key, p_entry->key))
        {
            return p_entry->value;
        }
    }

    return NULL;
}

void* unordered_map_remove(unordered_map* map, void* key)
{
    void*  value;
    size_t index;
    unordered_map_entry* prev_entry;
    unordered_map_entry* current_entry;

    if (!map) 
    {
        return NULL;
    }
    
    index = map->hash_function(key) & map->mask;

    prev_entry = NULL;

    for (current_entry = map->table[index];
         current_entry;
         current_entry = current_entry->chain_next)
    {
        if (map->equals_function(key, current_entry->key)) 
        {
            if (prev_entry)
            {
                /* Omit the 'p_current_entry' in the collision chain. */
                prev_entry->chain_next = current_entry->chain_next;
            }
            else
            {
                map->table[index] = current_entry->chain_next;
            }

            /* Unlink from the global iteration chain. */
            if (current_entry->prev)
            {
                current_entry->prev->next = current_entry->next;
            }
            else
            {
                map->head = current_entry->next;
            }
            
            if (current_entry->next)
            {
                current_entry->next->prev = current_entry->prev;
            }
            else
            {
                map->tail = current_entry->prev;
            }

            value = current_entry->value;
            map->size--;
            map->mod_count++;
            free(current_entry);
            return value;
        }

        prev_entry = current_entry;
    }

    return NULL;
}

void unordered_map_clear(unordered_map* map)
{
    unordered_map_entry* entry;
    unordered_map_entry* next_entry;
    size_t index;

    if (!map)
    {
        return;
    }
    
    entry = map->head;

    while (entry)
    {
        index = map->hash_function(entry->key) & map->mask;
        next_entry = entry->next;
        free(entry);
        entry = next_entry;
        map->table[index] = NULL;
    }

    map->mod_count += map->size;
    map->size = 0;
    map->head = NULL;
    map->tail = NULL;
}

size_t unordered_map_size(unordered_map* map)
{
    return map ? map->size : 0;
}

bool unordered_map_is_healthy(unordered_map* map)
{
    size_t counter;
    unordered_map_entry* entry;

    if (!map)
    {
        return false;
    }
    
    counter = 0;
    entry = map->head;

    if (entry && entry->prev) 
    {
        return false;
    }
    
    for (; entry; entry = entry->next)
    {
        counter++;
    }

    return counter == map->size;
}

void unordered_map_free(unordered_map* map)
{
    if (!map)
    {
        return;
    }
    
    unordered_map_clear(map);
    free(map->table);
    free(map);
}

unordered_map_iterator* 
unordered_map_iterator_alloc(unordered_map* map)
{
    unordered_map_iterator* p_ret;

    if (!map) 
    {
        return NULL;
    }
    
    p_ret = malloc(sizeof(*p_ret));

    if (!p_ret) 
    {
        return NULL;
    }
    
    p_ret->map              = map;
    p_ret->iterated_count     = 0;
    p_ret->next_entry       = map->head;
    p_ret->expected_mod_count = map->mod_count;

    return p_ret;
}

size_t unordered_map_iterator_has_next(unordered_map_iterator* iterator)
{
    if (!iterator) 
    {
        return 0;
    }
    
    if (unordered_map_iterator_is_disturbed(iterator)) 
    {
        return 0;
    }
    
    return iterator->map->size - iterator->iterated_count;
}

bool unordered_map_iterator_next(unordered_map_iterator* iterator, 
                                   void** key_pointer, 
                                   void** value_pointer)
{
    if (!iterator)   
    {
        return false;
    }
        
    if (!iterator->next_entry)         
    {
        return false;
    }
    
    if (unordered_map_iterator_is_disturbed(iterator))
    {
        return false;
    }
    
    *key_pointer   = iterator->next_entry->key;
    *value_pointer = iterator->next_entry->value;
    iterator->iterated_count++;
    iterator->next_entry = iterator->next_entry->next;
    
    return true;
}

bool unordered_map_iterator_is_disturbed(unordered_map_iterator* iterator)
{
    if (!iterator)
    {
        false;
    }
    
    return iterator->expected_mod_count != iterator->map->mod_count;
}

void unordered_map_iterator_free(unordered_map_iterator* iterator)
{
    if (!iterator) 
    {
        return;
    }
    
    iterator->map = NULL;
    iterator->next_entry = NULL;
    free(iterator);
}

void* unordered_map_first(unordered_map* map) {
    return map->head;
}
  
bool unordered_map_empty(unordered_map* map) {
    return map->size == 0;
}

size_t unordered_map_hash_32(uint32_t a)
{
	return a;
}

size_t unordered_map_hash_64(uint64_t a)
{
	return ((uint32_t) a) ^ (uint32_t) (a >> 32u);
}

// clang-format off (murmurhash)
size_t unordered_map_hash_str(const char *key)
{
	const uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
	const size_t len = strlen(key);
	const unsigned char* p = (const unsigned char*) key;
	const unsigned char *end = p + (len & ~(uint64_t) 0x7);
	uint64_t h = (len * m);

	while (p != end) {
		uint64_t k;
		memcpy(&k, p, sizeof(k));

		k *= m;
		k ^= k >> 47u;
		k *= m;

		h ^= k;
		h *= m;
		p += 8;
	}

	switch (len & 7u) {
	case 7: h ^= (uint64_t) p[6] << 48ul; // fall through
	case 6: h ^= (uint64_t) p[5] << 40ul; // fall through
	case 5: h ^= (uint64_t) p[4] << 32ul; // fall through
	case 4: h ^= (uint64_t) p[3] << 24ul; // fall through
	case 3: h ^= (uint64_t) p[2] << 16ul; // fall through
	case 2: h ^= (uint64_t) p[1] << 8ul;  // fall through
	case 1: h ^= (uint64_t) p[0];         // fall through
		h *= m;
	default:
		break;
	};

	h ^= h >> 47u;
	h *= m;
	h ^= h >> 47u;

	return (uint32_t) h;
}
