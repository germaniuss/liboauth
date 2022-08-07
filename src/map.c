#include "map.h"
#include <stdlib.h>
#include <stdbool.h>

typedef struct map_entry {
    void*             key;
    void*             value;
    struct map_entry* left;
    struct map_entry* right;
    struct map_entry* parent;
    int               height;
} map_entry;

struct map {
    map_entry* root;
    int      (*comparator)(void*, void*);
    size_t     size;
    size_t     mod_count;
};

struct map_iterator {
    map*       owner_map;
    map_entry* next;
    size_t     iterated_count;
    size_t     expected_mod_count;
};
    
/*******************************************************************************
* Creates a new map entry and initializes its fields.                          *
*******************************************************************************/  
static map_entry* map_entry_t_alloc(void* key, void* value) 
{
    map_entry* entry = malloc(sizeof(*entry));

    if (!entry) 
    {
        return NULL;
    }
    
    entry->key    = key;
    entry->value  = value;
    entry->left   = NULL;
    entry->right  = NULL;
    entry->parent = NULL;
    entry->height = 0;

    return entry;
}

/*******************************************************************************
* Returns the height of an entry. The height of a non-existent entry is        *
* assumed to be -1.                                                            *
*******************************************************************************/
static int height(map_entry* node) 
{
    return node ? node->height : -1;
}

/*******************************************************************************
* Returns the maximum of the two input integers.                               *
*******************************************************************************/
static int max(int a, int b) 
{
    return a > b ? a : b;
}

/*******************************************************************************
* Performs a left rotation and returns the new root of a (sub)tree.            *
*******************************************************************************/
static map_entry* left_rotate(map_entry* node_1)
{
    map_entry* node_2 = node_1->right;
    
    node_2->parent    = node_1->parent;
    node_1->parent    = node_2;
    node_1->right     = node_2->left;
    node_2->left      = node_1;

    if (node_1->right) 
    {
        node_1->right->parent = node_1;
    }
    
    node_1->height = max(height(node_1->left), height(node_1->right)) + 1;
    node_2->height = max(height(node_2->left), height(node_2->right)) + 1;
    
    return node_2;
}

/*******************************************************************************
* Performs a right rotation and returns the new root of a (sub)tree.           *
*******************************************************************************/
static map_entry* right_rotate(map_entry* node_1)
{
    map_entry* node_2 = node_1->left;
    
    node_2->parent   = node_1->parent;
    node_1->parent   = node_2;
    node_1->left     = node_2->right;
    node_2->right    = node_1;

    if (node_1->left) 
    {
        node_1->left->parent = node_1;
    }
    
    node_1->height = max(height(node_1->left), height(node_1->right)) + 1;
    node_2->height = max(height(node_2->left), height(node_2->right)) + 1;
    
    return node_2;
}

/*******************************************************************************
* Performs a right rotation following by a left rotation and returns the root  *
* of the new (sub)tree.                                                        * 
*******************************************************************************/
static map_entry* right_left_rotate(map_entry* node_1) 
{
    map_entry* node_2 = node_1->right;
    
    node_1->right = right_rotate(node_2);
    
    return left_rotate(node_1);
}

/*******************************************************************************
* Performs a left rotation following by a right rotation and returns the root  *
* of the new (sub)tree.                                                        * 
*******************************************************************************/
static map_entry* left_right_rotate(map_entry* node_1)
{
    map_entry* node_2 = node_1->left;
    
    node_1->left = left_rotate(node_2);
    
    return right_rotate(node_1);
}

/*******************************************************************************
* Fixes the tree in order to balance it. Basically, we start from 'p_entry'    *
* go up the chain towards parents. If a parent is disbalanced, a set of        *
* rotations are applied. If 'insertion_mode' is on, it means that previous     *  
* modification was insertion of an entry. In such a case we need to perform    *
* only one rotation. If 'insertion_mode' is off, the last operation was        *
* removal and we need to go up until the root node.                            *
*******************************************************************************/  
static void fix_after_modification(map* p_map, 
                                   map_entry* entry,
                                   bool insertion_mode)
{
    map_entry* parent = entry->parent;
    map_entry* grand_parent;
    map_entry* sub_tree;

    while (parent) 
    {
        if (height(parent->left) == height(parent->right) + 2)
        {
            grand_parent = parent->parent;

            if (height(parent->left->left) >= height(parent->left->right)) 
            {
                sub_tree = right_rotate(parent);
            }
            else
            {
                sub_tree = left_right_rotate(parent);
            }
                
            if (!grand_parent) 
            {
                p_map->root = sub_tree;
            }
            else if (grand_parent->left == parent) 
            {
                grand_parent->left = sub_tree;
            }
            else
            {
                grand_parent->right = sub_tree;
            }
             
            if (grand_parent)
            {
                grand_parent->height = 
                        max(height(grand_parent->left),
                                   height(grand_parent->right)) + 1;
            }   

            /* Fixing after insertion requires only one rotation. */
            if (insertion_mode) 
            {
                return;
            }
        }
        else if (height(parent->right) == height(parent->left) + 2) 
        {
            grand_parent = parent->parent;

            if (height(parent->right->right) >= height(parent->right->left)) 
            {
                sub_tree = left_rotate(parent);
            }
            else
            {
                sub_tree = right_left_rotate(parent);
            }
                
            if (!grand_parent)
            {
                p_map->root = sub_tree;
            }
            else if (grand_parent->left == parent)
            {
                grand_parent->left = sub_tree;
            }
            else
            {
                grand_parent->right = sub_tree;
            }
             
            if (grand_parent)
            {   
                grand_parent->height = 
                        max(height(grand_parent->left),
                            height(grand_parent->right)) + 1;
            }

            /* Fixing after insertion requires only one rotation. */
            if (insertion_mode) 
            {
                return;
            }
        }

        parent->height = max(height(parent->left),
                             height(parent->right)) + 1;
        parent = parent->parent;
    }
}

/*******************************************************************************
* Performs the actual insertion of an entry.                                   *
*******************************************************************************/
static void insert(map* my_map, void* key, void* value) 
{
    map_entry* new_entry = map_entry_t_alloc(key, value);
    map_entry* x;
    map_entry* parent;

    if (!new_entry)
    {
        return;
    }
    
    if (!my_map->root)
    {
        my_map->root = new_entry;
        my_map->size++;
        my_map->mod_count++;
        
        return;
    }

    x = my_map->root;
    parent = NULL;

    while (x) 
    {
        parent = x;

        if (my_map->comparator(new_entry->key, x->key) < 0)
        {
            x = x->left;
        }
        else
        {
            x = x->right;
        }
    }

    new_entry->parent = parent;

    if (my_map->comparator(new_entry->key, parent->key) < 0) 
    {
        parent->left = new_entry;
    }
    else
    {
        parent->right = new_entry;
    }

    /** TRUE means we choose the insertion mode for fixing the tree. */
    fix_after_modification(my_map, new_entry, true);
    my_map->size++;
    my_map->mod_count++;
}

/*******************************************************************************
* Returns the minimum entry of a subtree rooted at 'p_entry'.                  *
*******************************************************************************/  
static map_entry* min_entry(map_entry* entry)
{
    while (entry->left) 
    {
        entry = entry->left;
    }
    
    return entry;
}

/*******************************************************************************
* Returns the successor entry as specified by the order implied by the         *
* comparator.                                                                  *
*******************************************************************************/
static map_entry* get_successor_entry(map_entry* entry)
{
    map_entry* parent;

    if (entry->right) 
    {
        return min_entry(entry->right);
    }
    
    parent = entry->parent;

    while (parent && parent->right == entry)
    {
        entry = parent;
        parent = parent->parent;
    }

    return parent;
}

/*******************************************************************************
* This routine is responsible for removing entries from the tree.              *
*******************************************************************************/  
static map_entry* delete_entry(map* my_map, map_entry* entry)
{
    map_entry* parent;
    map_entry* child;
    map_entry* successor;

    void* tmp_key;
    void* tmp_value;

    if (!entry->left && !entry->right)
    {
        /** The node to delete has no children. */
        parent = entry->parent;

        if (!parent) 
        {
            my_map->root = NULL;
            my_map->size--;
            my_map->mod_count++;
            
            return entry;
        }

        if (entry == parent->left) 
        {
            parent->left = NULL;
        }
        else
        {
            parent->right = NULL;
        }

        my_map->size--;
        my_map->mod_count++;
        
        return entry;
    }

    if (!entry->left || !entry->right)
    {
        /** The node has exactly one child. */
        if (entry->left)
        {
            child = entry->left;
        }
        else
        {
            child = entry->right;
        }

        parent = entry->parent;
        child->parent = parent;

        if (!parent) 
        {
            my_map->root = child;
            my_map->size--;
            my_map->mod_count++;
            
            return entry;
        }

        if (entry == parent->left)
        {
            parent->left = child;
        }
        else 
        {
            parent->right = child;
        }

        my_map->size--;
        my_map->mod_count++;
        
        return entry;
    }

    /** The node to remove has both children. */
    tmp_key        = entry->key;
    tmp_value      = entry->value;
    successor      = min_entry(entry->right);
    entry->key     = successor->key;
    entry->value   = successor->value;
    child          = successor->right;
    parent         = successor->parent;

    if (parent->left == successor)
    {
        parent->left = child;
    }   
    else
    {
        parent->right = child;
    }
 
    if (child)
    {
        child->parent = parent;
    }
        
    my_map->size--;
    my_map->mod_count++;
    successor->key   = tmp_key;
    successor->value = tmp_value;
    
    return successor;
}

/*******************************************************************************
* Searches for an entry with key 'key'. Returns NULL if there is no such.      *
*******************************************************************************/  
static map_entry* find_entry(map* my_map, void* key)
{
    map_entry* p_entry = my_map->root;

    while (p_entry && my_map->comparator(key, p_entry->key) != 0)
    {
        if (my_map->comparator(key, p_entry->key) < 0)
        {
            p_entry = p_entry->left;
        }
        else
        {
            p_entry = p_entry->right;
        }
    }

    return p_entry;
}

map* map_alloc(int (*comparator)(void*, void*)) 
{
    map* my_map;

    if (!comparator) 
    {
        return NULL;
    }
    
    my_map = malloc(sizeof(*my_map));

    if (!my_map) 
    {
        return NULL;
    }
    
    my_map->root = NULL;
    my_map->comparator = comparator;
    my_map->size = 0;
    my_map->mod_count = 0;

    return my_map;
}

void* map_put(map* my_map, void* key, void* value)
{
    map_entry* target;
    void* old_value;

    if (!my_map) 
    {
        return NULL;
    }
    
    target = find_entry(my_map, key);

    if (target)
    {
        old_value = target->value;
        target->value = value;
        
        return old_value; 
    } 

    insert(my_map, key, value);
    return NULL;
}

bool map_contains_key (map* my_map, void* key)
{
    if (!my_map) 
    {
        return false;
    }

    return find_entry(my_map, key);
}

void* map_get(map* my_map, void* key)
{
    map_entry* entry;

    if (!my_map) 
    {
        return NULL;
    }
    
    entry = find_entry(my_map, key);
    
    return entry ? entry->value : NULL;
}

void* map_remove(map* my_map, void* key)
{
    void* value;
    map_entry* entry;

    if (!my_map) 
    {
        return NULL;
    }
    
    entry = find_entry(my_map, key);

    if (!entry) 
    {
        return NULL;
    }
    
    value = entry->value;
    entry = delete_entry(my_map, entry);
    fix_after_modification(my_map, entry, false);
    free(entry);
    
    return value;
}

/*******************************************************************************
* This routine implements the actual checking of tree balance.                 *
*******************************************************************************/  
static bool check_balance_factors_impl(map_entry* entry)
{
    if (!entry) 
    {
        return true;
    }
    
    if (abs(height(entry->left) - height(entry->right)) > 1)        
    {
        return false;
    }
    
    if (!check_balance_factors_impl(entry->left)) 
    {
        return false;
    }
    
    if (!check_balance_factors_impl(entry->right)) 
    {
        return false;
    }
    
    return true;
}

/*******************************************************************************
* Checks that every node in the map is balanced.                               *
*******************************************************************************/  
static int check_balance_factors(map* my_map) 
{
    return check_balance_factors_impl(my_map->root);
}

/*******************************************************************************
* This routine implements the actual height verification algorithm. It uses a  *
* sentinel value of -2 for denoting the fact that a current subtree contains   *
* at least one unbalanced node.                                                *  
*******************************************************************************/  
static int check_heights_impl(map_entry* entry)
{
    int height_left;
    int height_right;
    int height_both;

    /**********************************************************
    * The base case: the height of a non-existent leaf is -1. *
    **********************************************************/ 
    if (!entry)
    {
        return -1;
    }
    
    height_left = check_heights_impl(entry->left) + 1;

    if (height_left == -2)
    {
        return -2;
    }
    
    height_right = check_heights_impl(entry->right) + 1;

    if (height_right == -2)
    {
        return -2;
    }
    
    if ((height_both = max(height_left, 
                           height_right)) != entry->height)
    {
        return -2;
    }
    
    return height_both;
}

/*******************************************************************************
* This routine checks that the height field of each map entry (node) is        *
* correct.                                                                     *
*******************************************************************************/  
static int check_heights(map* my_map)
{
    return check_heights_impl(my_map->root) != -2;
}

bool map_is_healthy(map* my_map) 
{
    if (!my_map) 
    {
        return false;
    }
    
    if (!check_heights(my_map)) 
    {
        return false;
    }
    
    return check_balance_factors(my_map);
}

/*******************************************************************************
* Implements the actual deallocation of the tree entries by traversing the     *
* tree in post-order.                                                          * 
*******************************************************************************/  
static void map_free_impl(map_entry* entry)
{
    if (!entry)
    {
        return;
    }
    
    map_free_impl(entry->left);
    map_free_impl(entry->right);
    free(entry);
}

void map_free(map* my_map) 
{
    if (!my_map || !my_map->root)      
    {
        return;
    }
    
    map_free_impl(my_map->root);
    free(my_map);
}

void map_clear(map* my_map) 
{
    if (!my_map || !my_map->root) 
    {
        return;
    }
    
    map_free_impl(my_map->root);
    my_map->mod_count += my_map->size;
    my_map->root = NULL;
    my_map->size = 0;
}

size_t map_size(map* my_map) 
{
    return my_map ? my_map->size : 0;
}

map_iterator* map_iterator_alloc(map* my_map)
{
    map_iterator* iterator;
    
    if (!my_map) 
    {
        return NULL;
    }
    
    iterator = malloc(sizeof(*iterator));
    iterator->expected_mod_count = my_map->mod_count;
    iterator->iterated_count = 0;
    iterator->owner_map = my_map;
    iterator->next = my_map->root ? min_entry(my_map->root) : NULL;
    
    return iterator;
}

size_t map_iterator_has_next(map_iterator* iterator) 
{
    if (!iterator)
    {
        return 0;
    }
    
    /** If the map was modified, stop iteration. */
    if (map_iterator_is_disturbed(iterator)) 
    {
        return 0;
    }
    
    return iterator->owner_map->size - iterator->iterated_count;
}

bool map_iterator_next(map_iterator* iterator, 
                         void** key_pointer, 
                         void** value_pointer)
{
    if (!iterator)    
    {
        return false;
    }
    
    if (!iterator->next)     
    {
        return false;
    }
    
    if (map_iterator_is_disturbed(iterator))
    {
        return false;
    }
    
    *key_pointer   = iterator->next->key;
    *value_pointer = iterator->next->value;
    iterator->iterated_count++;
    iterator->next = get_successor_entry(iterator->next);
    
    return true;
}

bool map_iterator_is_disturbed(map_iterator* iterator) 
{
    if (!iterator) 
    {
        return false;
    }
    
    return iterator->expected_mod_count != iterator->owner_map->mod_count;
}

void map_iterator_free(map_iterator* iterator) 
{
    if (!iterator) 
    {
        return;
    }
    
    iterator->owner_map = NULL;
    iterator->next = NULL;
    free(iterator);
}
