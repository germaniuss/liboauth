#ifndef MAP_H
#define	MAP_H

#include <stdlib.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif
    
    typedef struct map map;
    typedef struct map_iterator map_iterator;

    /***************************************************************************
    * Allocates a new, empty map with given comparator function.               *
    ***************************************************************************/ 
    map* map_alloc (int (*comparator)(void*, void*));

    /***************************************************************************
    * If the map does not contain the key, inserts it in the map and associates*
    * the value with it, returning NULL. Otherwise, the value is updated and   *
    * the old value is returned.                                               *  
    ***************************************************************************/ 
    void* map_put (map* my_map, void* key, void* value);

    /***************************************************************************
    * Returns true if the key is mapped in the map.                            *
    ***************************************************************************/
    bool map_contains_key (map* my_map, void* key);

    /***************************************************************************
    * Returns the value associated with the key, or NULL if the key is not     *
    * mapped in the map.                                                       *
    ***************************************************************************/
    void* map_get (map* my_map, void* key);

    /***************************************************************************
    * If the key is mapped in the map, removes the mapping and returns the value*
    * of that mapping. If the map did not contain the mapping, returns NULL.   *
    ***************************************************************************/ 
    void* map_remove (map* my_map, void* key);

    /***************************************************************************
    * Removes all the contents of the map. Deallocates the map structures. The *
    * client user is responsible for deallocating the actual contents.         *  
    ***************************************************************************/ 
    void map_clear (map* my_map);

    /***************************************************************************
    * Returns the size of the map, or namely, the amount of key/value mappings *
    * in the map.                                                              *
    ***************************************************************************/ 
    size_t map_size (map* my_map);

    /***************************************************************************
    * Checks that the map maintains the AVL-tree property.                     *
    ***************************************************************************/  
    bool map_is_healthy (map* my_map);

    /***************************************************************************
    * Deallocates the entire map. Only the map and its nodes are deallocated.  *
    * The user is responsible for deallocating the actual data stored in the   * 
    * map.                                                                     *
    ***************************************************************************/ 
    void map_free (map* my_map);

    /***************************************************************************
    * Returns the iterator over the map. The entries are iterated in order.    *
    ***************************************************************************/  
    map_iterator* map_iterator_alloc (map* my_map);

    /***************************************************************************
    * Returns the number of keys not yet iterated over.                        *
    ***************************************************************************/ 
    size_t map_iterator_has_next (map_iterator* iterator);

    /***************************************************************************
    * Loads the next mapping in the iteration order.                           *
    ***************************************************************************/  
    bool map_iterator_next (map_iterator* iterator, 
                            void** key_pointer, 
                            void** value_pointer);

    /***************************************************************************
    * Returns a true if the map was modified during the iteration.             *
    ***************************************************************************/  
    bool map_iterator_is_disturbed (map_iterator* iterator);

    /***************************************************************************
    * Deallocates the map iterator.                                            *
    ***************************************************************************/  
    void map_iterator_free (map_iterator* iterator);

#ifdef	__cplusplus
}
#endif

#endif	/* MAP_H */

