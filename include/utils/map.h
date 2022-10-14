#ifndef _UTILS_MAP_H
#define _UTILS_MAP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "hash.h"

#define map_calloc calloc
#define map_free free

#ifndef MAP_MAX
#define MAP_MAX UINT32_MAX
#endif		

#define map_dec_strkey(name, K, V) \
	struct map_item_##name                      \
	{                                           \
		K key;                                  \
		V value;                                \
		uint32_t hash;                          \
	};                                          \
                                                \
	map_of(name, K, V)

#define map_dec_scalar(name, K, V) \
	struct map_item_##name                      \
	{                                           \
		K key;                                  \
		V value;                                \
	};                                          \
                                                \
	map_of(name, K, V)

#define map_of(name, K, V)                                  \
                                                                         \
	struct map_link_##name                                               \
	{                                                                    \
		struct map_link_##name *next;                                    \
		struct map_link_##name *prev;                                    \
		struct map_item_##name *entry;                                   \
	};                                                                   \
                                                                         \
	struct map_##name                                                    \
	{                                                                    \
		struct map_item_##name *mem;                                     \
		struct map_link_##name *list;                                    \
		struct map_link_##name *head;                                    \
		struct map_link_##name *tail;                                    \
		uint32_t max_size;                                               \
		uint32_t cap;                                                    \
		uint32_t size;                                                   \
		uint32_t load_fac;                                               \
		uint32_t remap;                                                  \
		bool used;                                                       \
		bool oom;                                                        \
		bool found;                                                      \
		bool circular;                                                   \
		bool refresh;                                                    \
	};                                                                   \
                                                                         \
	/**                                                                  \
	 * Create map                                                        \
	 *                                                                   \
	 * @param map map                                                    \
	 * @param cap initial capacity, zero is accepted                     \
	 * @param load_factor must be >25 and <95. Pass 0 for default value. \
	 * @return 'true' on success,                                        \
	 *         'false' on out of memory or if 'load_factor' value is     \
	 *          invalid.                                                 \
	 */                                                                  \
	bool map_init_##name(struct map_##name *map, uint32_t cap,           \
						 uint32_t load_factor);                          \
                                                                         \
	/**                                                                  \
	 * Destroy map.                                                      \
	 *                                                                   \
	 * @param map map                                                    \
	 */                                                                  \
	void map_term_##name(struct map_##name *map);                        \
                                                                         \
	/**                                                                  \
	 * Get map element count                                             \
	 *                                                                   \
	 * @param map map                                                    \
	 * @return element count                                             \
	 */                                                                  \
	uint32_t map_size_##name(struct map_##name *map);                    \
                                                                         \
	/**                                                                  \
	 * Clear map                                                         \
	 *                                                                   \
	 * @param map map                                                    \
	 */                                                                  \
	void map_clear_##name(struct map_##name *map);                       \
                                                                         \
	/**                                                                  \
	 * Put element to the map                                            \
	 *                                                                   \
	 * struct map_str map;                                               \
	 * map_put_str(&map, "key", "value");                                \
	 *                                                                   \
	 * @param map map                                                    \
	 * @param K key                                                      \
	 * @param V value                                                    \
	 * @return previous value if exists                                  \
	 *         call map_found() to see if the returned value is valid.   \
	 */                                                                  \
	V map_put_##name(struct map_##name *map, K key, V val);              \
                                                                         \
	/**                                                                  \
	 * Get element                                                       \
	 *                                                                   \
	 * @param map map                                                    \
	 * @param K key                                                      \
	 * @return current value if exists.                                  \
	 *         call map_found() to see if the returned value is valid.   \
	 */                                                                  \
	/** NOLINTNEXTLINE */                                                \
	V map_get_##name(struct map_##name *map, K key);                     \
                                                                         \
	/**                                                                  \
	 * Delete element                                                    \
	 *                                                                   \
	 * @param map map                                                    \
	 * @param K key                                                      \
	 * @return current value if exists.                                  \
	 *         call map_found() to see if the returned value is valid.   \
	 */                                                                  \
	/** NOLINTNEXTLINE */                                                \
	V map_del_##name(struct map_##name *map, K key);                     \
                                                                         \
	void map_refresh_or_add_##name(struct map_##name *map, bool refresh, struct map_link_##name *link);

/**
 * @param map map
 * @return    - if put operation overrides a value, returns true
 *            - if get operation finds the key, returns true
 *            - if del operation deletes a key, returns true
 */
#define map_found(map) ((map)->found)

/**
 * @param map map
 * @return    true if put operation failed with out of memory
 */
#define map_oom(map) ((map)->oom)

#define map_set_max_size(map, v)										\
																		\
	if ((v) <= 0 || (v) > MAP_MAX * (map)->load_fac / 100)				\
	{																	\
		(map)->max_size = 0;											\
	}																	\
																		\
	(map)->max_size = (v)

#define map_set_refresh(map, v) ((map)->refresh = (v))
#define map_set_circular(map, v) ((map)->circular = (v))

// clang-format off

/**
 * Foreach loop
 *
 * char *key, *value;
 * struct map_str map;
 *
 * map_foreach(&map, key, value) {
 *      printf("key = %s, value = %s \n");
 * }
 */
#define map_foreach(map, K, V)                                             \
	for (int64_t _i = -1, _b = 0; !_b && _i < (map)->cap; _i++)            \
		for ((V) = (map)->mem[_i].value, (K) = (map)->mem[_i].key, _b = 1; \
		     _b && ((_i == -1 && (map)->used) || (K) != 0) ? 1 : (_b = 0); \
		     _b = 0)

/**
 * Foreach loop for keys
 *
 * char *key;
 * struct map_str map;
 *
 * map_foreach_key(&map, key) {
 *      printf("key = %s \n");
 * }
 */
#define map_foreach_key(map, K)                                                 \
	for (int64_t _i = -1, _b = 0; !_b && _i < (map)->cap; _i++)                \
		for ((K) = (map)->mem[_i].key, _b = 1;                             \
		     _b && ((_i == -1 && (map)->used) || (K) != 0) ? 1 : (_b = 0); \
		     _b = 0)

/**
 * Foreach loop for values
 *
 * char *value;
 * struct map_str map;
 *
 * map_foreach_value(&map, value) {
 *      printf("value = %s \n");
 * }
 */
#define map_foreach_value(map, V)                                                              \
	for (int64_t _i = -1, _b = 0; !_b && _i < (map)->cap; _i++)                               \
		for ((V) = (map)->mem[_i].value, _b = 1;                                          \
		     _b && ((_i == -1 && (map)->used) || (map)->mem[_i].key != 0) ? 1 : (_b = 0); \
		     _b = 0)

//              name  key type      value type
map_dec_scalar(32,  uint32_t,     uint32_t)
map_dec_scalar(64,  uint64_t,     uint64_t)
map_dec_scalar(64v, uint64_t,     void *)
map_dec_scalar(64s, uint64_t,     const char *)
map_dec_strkey(str, const char *, const char *)
map_dec_strkey(sv,  const char *, void*)
map_dec_strkey(s64, const char *, uint64_t)

#define map_def_strkey(name, K, V, cmp, hash_fn, empty_value)               \
	bool map_cmp_##name(struct map_item_##name *t, K key,            \
			       uint32_t hash)                                  \
	{                                                                      \
		return t->hash == hash && cmp(t->key, key);                    \
	}                                                                      \
                                                                               \
	void map_assign_##name(uint32_t pos, K key,         \
				  V value, uint32_t hash, struct map_##name *m)                      \
	{                                                                   \
		m->mem[pos].hash = hash;                                        \
		m->mem[pos].key = key;                                          \
		m->mem[pos].value = value;                                      \
		m->list[pos].entry = &m->mem[pos];								\
		map_refresh_or_add_##name(m, m->found, &m->list[pos]);			\
	}                                                                      		\
                                                                               	\
	uint32_t map_hashof_##name(struct map_item_##name *t)            \
	{                                                                      \
		return t->hash;                                                \
	}                                                                      \
                                                                               \
	map_def(name, K, V, cmp, hash_fn, empty_value)

#define map_def_scalar(name, K, V, cmp, hash_fn, empty_value)         \
	bool map_cmp_##name(struct map_item_##name *t, K key,            \
			       uint32_t hash)                                  \
	{                                                                      \
		(void) hash;                                                   \
		return cmp(t->key, key);                                       \
	}                                                                      \
                                                                               \
	void map_assign_##name(uint32_t pos, K key,         \
				  V value, uint32_t hash, struct map_##name *m)                      \
	{                                                                      \
		(void) hash;                                                   \
		m->mem[pos].key = key;                                                  \
		m->mem[pos].value = value;                                              \
		m->list[pos].entry = &m->mem[pos];										\
		map_refresh_or_add_##name(m, m->found, &m->list[pos]);			\
	}                                                                      \
                                                                               \
	uint32_t map_hashof_##name(struct map_item_##name *t)            \
	{                                                                      \
		return hash_fn(t->key);                                        \
	}                                                                      \
                                                                               \
	map_def(name, K, V, cmp, hash_fn, empty_value)

#define map_def(name, K, V, cmp, hash_fn, empty_value)                         \
                                                                               \
	static const struct map_item_##name empty_items_##name[2];          \
                                                                               \
	static const struct map_##name map_empty_##name = {              \
		.cap = 1,                                                      \
		.mem = (struct map_item_##name *) &empty_items_##name[1],		\
		.head = 0,														\
		.tail = 0};  													\
                                                                               \
	static void map_remap_list_##name(struct map_item_##name* new, struct map_link_##name* new_list, int i, int pos, struct map_##name* m) {			\
		new[pos] = m->mem[i];          							\
		new_list[pos].entry = &new[pos];						\
		new_list[pos].next = m->list[i].next;					\
		if (new_list[pos].next) 								\
			new_list[pos].next->prev = &new_list[pos];			\
		else m->tail = &new_list[pos];							\
		new_list[pos].prev = m->list[i].prev;					\
		if (new_list[pos].prev)									\
			new_list[pos].prev->next = &new_list[pos];			\
		else m->head = &new_list[pos];							\
	}															\
																\
	void map_refresh_or_add_##name(struct map_##name *m, bool refresh, struct map_link_##name *link) {												\
		if (!refresh) {													\
			if (!m->head) {												\
				m->head = m->tail = link;								\
				link->prev = 0;											\
			} else {													\
				link->prev = m->tail;								\
				m->tail->next = link;								\
				m->tail = link;										\
			}															\
		} else if (m->refresh && link != m->tail) {						\
			if (link != m->head) {										\
				link->prev->next = link->next;							\
			} else {													\
				m->head = link->next;									\
			}															\
			link->next->prev = link->prev;								\
			link->prev = m->tail;										\
			m->tail->next = link;										\
			m->tail = link;												\
		}																		\
		link->next = 0;											\
	}																			\
																				\
	static void map_alloc_##name(uint32_t *cap, uint32_t factor, struct map_item_##name** t, struct map_link_##name **p)							\
	{                                                                   \
		uint32_t v = *cap;                                             	\
                                                                        \
		if (*cap > MAP_MAX / factor) {                              	\
			*t = NULL;													\
			*p = NULL;													\
			return;                                           	   		\
		}                                                              \
                                                                               \
		/* Find next power of two */                                   \
		v = v < 8 ? 8 : (v * factor);                                  \
		v--;                                                           \
		for (uint32_t i = 1; i < sizeof(v) * 8; i *= 2) {              \
			v |= v >> i;                                           		\
		}                                                              \
		v++;                                                           \
                                                                               \
		*cap = v;                                                      \
		*t = map_calloc(v + 1, sizeof(**t));                          \
		*t = *t ? &((*t)[1]) : NULL;									\
		*p = map_calloc(v + 1, sizeof(**p));								\
		*p = *p ? &((*p)[1]) : NULL;										\
	}                                                                      \
                                                                            \
	bool map_init_##name(struct map_##name *m, uint32_t cap,         \
				uint32_t load_fac)                             \
	{                                                                      \
		void *t;                                                       \
		void *p;                                                       \
		uint32_t f = (load_fac == 0) ? 75 : load_fac;                  \
                                                                               \
		if (f > 95 || f < 25) {                                        \
			return false;                                          \
		}                                                              \
                                                                               \
		if (cap == 0) {                                                \
			*m = map_empty_##name;                              	\
			m->load_fac = f;                                       \
			return true;                                           \
		}                                                              	\
                                                                     	\
		map_alloc_##name(&cap, 1, &t, &p);				                  \
		if (t == NULL || p == NULL) {                                    \
			return false;                                          		\
		}                                                              	\
                                                                               \
		m->mem = t;                                                    \
		m->list = p;														\
		m->size = 0;                                                   \
		m->used = false;                                               \
		m->cap = cap;                                                  \
		m->load_fac = f;                                               \
		m->remap = (uint32_t) (m->cap * ((double) m->load_fac / 100)); \
                                                                    	\
		return true;                                                   \
	}                                                                      \
                                                                               \
	void map_term_##name(struct map_##name *m)                       \
	{                                                                      \
		if (m->mem != map_empty_##name.mem) {                       \
			map_free(&m->mem[-1]);                              \
			map_free(&m->list[-1]);                             \
			*m = map_empty_##name;                              \
		}                                                              \
	}                                                                      \
                                                                               \
	uint32_t map_size_##name(struct map_##name *m)                   \
	{                                                                      \
		return m->size;                                                \
	}                                                                      \
                                                                               \
	void map_clear_##name(struct map_##name *m)                      \
	{                                                                      \
		if (m->size > 0) {                                             \
			for (uint32_t i = 0; i < m->cap; i++) {                \
				m->mem[i].key = 0;                             	\
				m->list[i].next = 0;							\
				m->list[i].prev = 0;							\
			}                                                   \
                                                                               \
			m->list[-1].next = 0;										\
			m->list[-1].prev = 0;										\
			m->used = false;                                       \
			m->size = 0;                                           \
		}                                                              \
	}                                                                      \
                                                                               \
	static bool map_remap_##name(struct map_##name *m, uint32_t* idx)          \
	{                                                                      \
		uint32_t pos, cap, mod;                                        \
		struct map_item_##name *new;                                \
		struct map_link_##name *new_list;                                \
                                                                               \
		if (m->size == m->max_size) {											\
			return false;														\
		}																		\
																				\
		if (m->size < m->remap) {                                      \
			return true;                                           \
		}                                                              \
                                                                               \
		cap = m->cap;                                                  \
		map_alloc_##name(&cap, 2, &new, &new_list);			                        \
		if (new == NULL || new_list == NULL) {                                             \
			return false;                                          \
		}                                                              \
                                                                               \
		mod = cap - 1;                                                 \
                                                                               \
		for (uint32_t i = 0; i < m->cap; i++) {                        \
			if (m->mem[i].key != 0) {                              				\
				pos = map_hashof_##name(&m->mem[i]) & mod;  					\
				if (i == idx) *idx = pos;										\
                                                                             	\
				while (true) {                                 					\
					if (new[pos].key == 0) {               						\
						map_remap_list_##name(new, new_list, i, pos, m);		\
						break;                         							\
					}                                      						\
                                                                               	\
					pos = (pos + 1) & (mod);               						\
				}                                              					\
			}                                                      				\
		}                                                              			\
                                                                               	\
		if (m->mem != map_empty_##name.mem) {                       			\
			if (m->used) map_remap_list_##name(new, new_list, -1, -1, m);		\
			map_free(&m->mem[-1]);                              				\
			map_free(&m->list[-1]);                              				\
		}                                                              			\
                                                                               	\
		m->mem = new;                                                  			\
		m->list = new_list;                                                  	\
		m->cap = cap;                                                  			\
		m->remap = (uint32_t) (m->cap * ((double) m->load_fac / 100)); 			\
                                                                               	\
		return true;                                                   			\
	}                                                                      		\
                                                                               	\
	V map_put_##name(struct map_##name *m, K key, V value)           			\
	{                                                                      		\
		V ret;                                                         	\
		uint32_t pos, mod, h;                                          	\
                                                                    	\
		m->oom = false;                                                	\
                                                                        \
		if (key == 0) {                                                	\
			ret = (m->used) ? m->mem[-1].value : (V) empty_value;    			\
			m->found = m->used;                                    \
			m->size += !m->used;                                   \
			m->used = true;                                        \
			map_assign_##name(-1, key, value, h, m);     			\
			return ret;                                            \
		}                                                              \
                                                                               \
		mod = m->cap - 1;                                              \
		h = hash_fn(key);                                              \
		pos = h & (mod);                                               \
                                                                               \
		while (true) {                                                 \
			if (m->mem[pos].key == 0 || map_cmp_##name(&m->mem[pos], key, h)) { 		\
				m->found = m->mem[pos].key != 0;                       		\
				if (!m->found && !map_remap_##name(m, &pos)) {             	\
					if (m->circular) {										\
						ret = map_del_##name(m, m->head->entry->key);		\
						m->found = false;									\
					} else {												\
						m->oom = true;                                      \
						return (V) empty_value;                             \
					}														\
				} else if (m->found) {										\
					ret = m->mem[pos].value;								\
				} else if (mod != (m->cap - 1)) {								\
					mod = m->cap - 1; 										\
					pos = h & (mod); 										\
					continue;												\
				}															\
				m->size += !m->found;										\
				map_assign_##name(pos, key, value, h, m);     				\
				return ret;                                            		\
			}                                                      		\
            pos = (pos + 1) & (mod);                       			\
		}                                                              	\
	}                                                                  	\
                                                                        \
	/** NOLINTNEXTLINE */                                                  \
	V map_get_##name(struct map_##name *m, K key)                    	\
	{                                                                  \
		const uint32_t mod = m->cap - 1;                               \
		uint32_t h, pos;                                               \
                                                                               \
		if (key == 0) {                                                \
			m->found = m->used;                                    \
			if (m->used) map_refresh_or_add_##name(m, true, &m->list[-1]);	\
			return m->used ? m->mem[-1].value : (V) empty_value;                 \
		}                                                              \
                                                                               \
		h = hash_fn(key);                                              \
		pos = h & mod;                                                 \
                                                                               \
		while (true) {                                                 \
			if (m->mem[pos].key == 0) {                            		\
				m->found = false;                              			\
				return (V) empty_value;                  				\
			} else if (!map_cmp_##name(&m->mem[pos], key, h)) { \
				m->found = true;                                       \
				map_refresh_or_add_##name(m, true, &m->list[pos]);		\
				return m->mem[pos].value;                              \
			}                                                      \
            pos = (pos + 1) & (mod);                       			\
		}                                                              \
	}                                                                      \
                                                                               \
	/** NOLINTNEXTLINE */                                                  \
	V map_del_##name(struct map_##name *m, K key)                    \
	{                                                                      \
		const uint32_t mod = m->cap - 1;                               \
		uint32_t pos, prev, it, p, h;                                  \
		V ret;                                                         \
                                                                               \
		if (key == 0) {                                                \
			m->found = m->used;                                    \
			m->size -= m->used;                                    \
			m->used = false;                                       \
																				\
			if (m->list[-1].prev)												\
				m->list[-1].prev->next = m->list[-1].next;						\
			else m->head = m->list[-1].next;									\
			if (m->list[-1].next)												\
				m->list[-1].next->prev = m->list[-1].prev;						\
			else m->tail = m->list[-1].prev;									\
			m->list[-1].entry = 0;												\
                                                                               \
			return m->found ? m->mem[-1].value : (V) empty_value;                \
		}                                                              \
                                                                               \
		h = hash_fn(key);                                              \
		pos = h & (mod);                                               \
                                                                               \
		while (true) {                                                 \
			if (m->mem[pos].key == 0) {                            \
				m->found = false;                              \
				return (V) empty_value;                                      \
			} else if (!map_cmp_##name(&m->mem[pos], key, h)) { \
				pos = (pos + 1) & (mod);                       \
				continue;                                      \
			}                                                      \
                                                                               \
			m->found = true;                                       \
			ret = m->mem[pos].value;                               \
                                                                               \
			m->size--;                                             \
			m->mem[pos].key = 0;                                   \
			if (m->list[pos].prev)												\
				m->list[pos].prev->next = m->list[pos].next;						\
			else m->head = m->list[pos].next;									\
			if (m->list[pos].next)												\
				m->list[pos].next->prev = m->list[pos].prev;						\
			else m->tail = m->list[pos].prev;									\
			m->list[pos].entry = 0;											\
			prev = pos;                                            \
			it = pos;                                              \
                                                                               \
			while (true) {                                         \
				it = (it + 1) & (mod);                         \
				if (m->mem[it].key == 0) {                     \
					break;                                 \
				}                                              \
                                                                               \
				p = map_hashof_##name(&m->mem[it]) & (mod); \
                                                                               \
				if ((p > it && (p <= prev || it >= prev)) ||   \
				    (p <= prev && it >= prev)) {               \
                                                                               \
					m->mem[prev] = m->mem[it];             \
					m->mem[it].key = 0;                    \
														\
					m->list[prev].entry = &m->mem[prev];								\
					m->list[prev].prev = m->list[it].prev;								\
					m->list[prev].next = m->list[it].next;								\
					if (m->list[it].prev)												\
						m->list[it].prev->next = &m->list[prev];						\
					else m->head = &m->list[prev];									\
					if (m->list[it].next)												\
						m->list[it].next->prev = &m->list[prev];					\
					else m->tail = &m->list[prev];									\
					m->list[it].entry = 0;											\
																					\
					prev = it;                             \
				}                                              \
			}                                                      \
                                                                               \
			return ret;                                            \
		}                                                              \
	}																																			

#ifdef __cplusplus
}
#endif

#if defined(_UTILS_IMPL) || defined(_UTILS_MAP_IMPL)

#include <string.h>										

//              name,  key type,   value type,     cmp           hash
map_def_scalar(32,  uint32_t,     uint32_t,     cmp_int,    hash_32, 0)
map_def_scalar(64,  uint64_t,     uint64_t,     cmp_int,    hash_64, 0)
map_def_scalar(64v, uint64_t,     void *,       cmp_int,    hash_64, 0)
map_def_scalar(64s, uint64_t,     const char *, cmp_int,    hash_64, 0)
map_def_strkey(str, const char *, const char *, cmp_str, murmurhash, 0)
map_def_strkey(sv,  const char *, void *,       cmp_str, murmurhash, 0)
map_def_strkey(s64, const char *, uint64_t,     cmp_str, murmurhash, 0)

#endif
#endif