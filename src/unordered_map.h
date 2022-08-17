#ifndef UNORDERED_MAP_H
#define	UNORDERED_MAP_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef	__cplusplus
extern "C" {
#endif

    typedef struct unordered_map          unordered_map;
    typedef struct unordered_map_iterator unordered_map_iterator;

    /***************************************************************************
    * Allocates a new, empty map with given hash function and given equality   *
    * testing function.                                                        * 
    ***************************************************************************/ 
    unordered_map* unordered_map_alloc 
           (size_t   initial_capacity,
            float    load_factor,
            size_t (*hash_function)(void*),
            bool   (*equals_function)(void*, void*));

    /***************************************************************************
    * If p_map does not contain the key p_key, inserts it in the map,          *
    * associates p_value with it and return NULL. Otherwise updates the value  *
    * and returns the old value.                                               * 
    ***************************************************************************/ 
    void* unordered_map_put (unordered_map* map, void* key, void* value);

    /***************************************************************************
    * Returns a positive value if p_key is mapped to some value in this map.   *
    ***************************************************************************/
    bool unordered_map_contains_key (unordered_map* map, void* key);

    /***************************************************************************
    * Returns the value associated with the p_key, or NULL if p_key is not     *
    * mapped in the map.                                                       *
    ***************************************************************************/
    void* unordered_map_get (unordered_map* map, void* key);

    /***************************************************************************
    * If p_key is mapped in the map, removes the mapping and returns the value *
    * of that mapping. If the map did not contain the mapping, returns NULL.   *
    ***************************************************************************/ 
    void* unordered_map_remove (unordered_map* map, void* p_key);

    /***************************************************************************
    * Removes all the contents of the map.                                     * 
    ***************************************************************************/ 
    void unordered_map_clear (unordered_map* map);

    /***************************************************************************
    * Returns the size of the map, or namely, the amount of key/value mappings *
    * in the map.                                                              *
    ***************************************************************************/ 
    size_t unordered_map_size (unordered_map* map);

    /***************************************************************************
    * Checks that the map is in valid state.                                   *
    ***************************************************************************/  
    bool unordered_map_is_healthy (unordered_map* map);

    /***************************************************************************
    * Deallocates the entire map. Only the map and its nodes are deallocated.  *
    * The user is responsible for deallocating the actual data stored in the   *
    * map.                                                                     *
    ***************************************************************************/ 
    void unordered_map_free (unordered_map* map);

    /***************************************************************************
    * Returns the iterator over the map. The entries are iterated in insertion *
    * order.                                                                   * 
    ***************************************************************************/  
    unordered_map_iterator* unordered_map_iterator_alloc
                           (unordered_map* map);

    /***************************************************************************
    * Returns the number of keys not yet iterated over.                        *
    ***************************************************************************/ 
    size_t unordered_map_iterator_has_next
          (unordered_map_iterator* iterator);

    /***************************************************************************
    * Loads the next entry in the iteration order.                             *
    ***************************************************************************/  
    bool unordered_map_iterator_next(unordered_map_iterator* iterator, 
                                     void** key_pointer, 
                                     void** value_pointer);

    /***************************************************************************
    * Returns a true if the map was modified during the iteration.             *
    ***************************************************************************/  
    bool unordered_map_iterator_is_disturbed
        (unordered_map_iterator* iterator);

    /***************************************************************************
    * Deallocates the map iterator.                                            *
    ***************************************************************************/  
    void unordered_map_iterator_free(unordered_map_iterator* iterator);

    /***************************************************************************
    * Returns the first element in queue.                                      *
    ***************************************************************************/  
    void* unordered_map_first(unordered_map* map);

    /***************************************************************************
    * Check if the map is empty.                                               *
    ***************************************************************************/  
    bool unordered_map_empty(unordered_map* map);

    /***************************************************************************
    * Define the hash functions.                                               *
    * *************************************************************************/
    size_t unordered_map_hash_32(uint32_t a);
    size_t unordered_map_hash_64(uint64_t a);
    size_t unordered_map_hash_str(const char *key);

    static inline bool unordered_map_eq(uint64_t a, uint64_t b) {return (a) == (b);}
    static inline bool unordered_map_streq(const char* a, const char* b) {return !strcmp(a, b);}

#ifdef	__cplusplus
}
#endif

#endif	/* UNORDERED_MAP_H */

