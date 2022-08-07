/*
 * BSD-3-Clause
 *
 * Copyright 2021 Ozan Tezcan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_VERSION "2.0.0"

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define queue_calloc calloc
#define queue_free free
#endif

#ifndef QUEUE_MAX
#define QUEUE_MAX (SIZE_MAX)
#endif

#define queue_def(T, name)                                                  \
	struct queue_##name {                                               \
		bool oom;                                                      \
		size_t cap;                                                    \
		size_t first;                                                  \
		size_t last;                                                   \
		/* NOLINTNEXTLINE */                                           \
		T *elems;                                                      \
	}

#define queue_expand(q)                                                     \
	do {                                                                   \
		size_t _cap, _len, _off;                                       \
		size_t _pos = ((q)->last + 1) & ((q)->cap - 1);                \
		void *_dst, *_src;                                             \
                                                                               \
		if (_pos == (q)->first) {                                      \
			if ((q)->cap > QUEUE_MAX / 2ul) {                   \
				(q)->oom = true;                               \
				break;                                         \
			}                                                      \
			_cap = (q)->cap * 2;                                   \
			_dst = queue_calloc(_cap, sizeof(*((q)->elems)));   \
			if (_dst == NULL) {                                    \
				(q)->oom = true;                               \
				break;                                         \
			}                                                      \
			_len = ((q)->cap - (q)->first) * sizeof(*(q)->elems);  \
			_off = ((q)->first * sizeof(*((q)->elems)));           \
			_src = ((char *) (q)->elems) + _off;                   \
                                                                               \
			memcpy(_dst, _src, _len);                              \
			memcpy(((char *) _dst) + _len, (q)->elems, _off);      \
			(q)->oom = false;                                      \
			(q)->last = (q)->cap - 1;                              \
			(q)->first = 0;                                        \
			(q)->cap = _cap;                                       \
			queue_free((q)->elems);                             \
			(q)->elems = _dst;                                     \
		}                                                              \
	} while (0)

/**
 *   Init queue. Call queue_oom(q) to see if memory allocation succeeded.
 *   @param q queue
 */
#define queue_init(q)                                                       \
	do {                                                                   \
		(q)->oom = false;                                              \
		(q)->cap = 8;                                                  \
		(q)->first = 0;                                                \
		(q)->last = 0;                                                 \
		(q)->elems = queue_calloc(1, sizeof(*(q)->elems) * 8);      \
		if ((q)->elems == NULL) {                                      \
			(q)->oom = true;                                       \
		}                                                              \
	} while (0)

/**
 *   Term queue
 *   @param q queue
 */
#define queue_term(q)                                                       \
	do {                                                                   \
		queue_free((q)->elems);                                     \
		(q)->elems = NULL;                                             \
		(q)->cap = 0;                                                  \
		(q)->first = 0;                                                \
		(q)->last = 0;                                                 \
		(q)->oom = false;                                              \
	} while (0)

/**
 * @param q queue
 * @return true if last add operation failed, false otherwise.
 */
#define queue_oom(q) ((q)->oom)

/**
 *   @param q queue
 *   @return  element count
 */
#define queue_size(q) (((q)->last - (q)->first) & ((q)->cap - 1))

/**
 * Clear the queue without deallocating underlying memory.
 * @param q queue
 */
#define queue_clear(q)                                                      \
	do {                                                                   \
		(q)->first = 0;                                                \
		(q)->last = 0;                                                 \
		(q)->oom = false;                                              \
	} while (0)

/**
 *   @param q queue
 *   @return true if queue is empty
 */
#define queue_empty(q) (((q)->last == (q)->first))

/**
 * @param q queue
 * @return  index of the first element. If queue is empty, result is undefined.
 */
#define queue_first(q) ((q)->first)

/**
 * @param q queue
 * @return  index of the last element. If queue is empty, result is undefined.
 */
#define queue_last(q) ((q)->last)

/**
 * @param q queue
 * @param i index
 * @return  index of the next element after i, if i is the last element,
 *            result is undefined.
 */
#define queue_next(q, i) (((i) + 1) & ((q)->cap - 1))

/**
 *   Returns element at index 'i', so regular loops are possible :
 *
 *   for (size_t i = 0; i < queue_size(q); i++) {
 *        printf("%d" \n, queue_at(q, i));
 *   }
 *
 *   @param q queue
 *   @return element at index i
 */
#define queue_at(q, i) (q)->elems[(((q)->first) + (i)) & ((q)->cap - 1)]

/**
 *   @param q queue
 *   @return  peek first element, if queue is empty, result is undefined
 */
#define queue_peek_first(q) ((q)->elems[(q)->first])

/**
 *   @param q queue
 *   @return  peek last element, if queue is empty, result is undefined
 */
#define queue_peek_last(q) (q)->elems[((q)->last - 1) & ((q)->cap - 1)]

/**
 * Call queue_oom(q) after this function to check out of memory condition.
 *
 * @param q    queue
 * @param elem elem to be added at the end of the list
 */
#define queue_add_last(q, elem)                                             \
	do {                                                                   \
		queue_expand(q);                                            \
		if ((q)->oom) {                                                \
			break;                                                 \
		}                                                              \
		(q)->oom = false;                                              \
		(q)->elems[(q)->last] = elem;                                  \
		(q)->last = ((q)->last + 1) & ((q)->cap - 1);                  \
	} while (0)

/**
 * @param q queue
 * @return  delete the last element from the queue and return its value.
 *          If queue is empty, result is undefined.
 */
#define queue_del_last(q)                                                   \
	((q)->elems[((q)->last = ((q)->last - 1) & ((q)->cap - 1))])

/**
 * Call queue_oom(q) after this function to check out of memory condition.
 *
 * @param q    queue.
 * @param elem elem to be added at the head of the list.
 */
#define queue_add_first(q, elem)                                            \
	do {                                                                   \
		queue_expand(q);                                            \
		if ((q)->oom) {                                                \
			break;                                                 \
		}                                                              \
		(q)->oom = false;                                              \
		(q)->first = ((q)->first - 1) & ((q)->cap - 1);                \
		(q)->elems[(q)->first] = elem;                                 \
	} while (0)

static inline size_t queue_inc_first(size_t *first, size_t cap)
{
	size_t tmp = *first;

	*first = (*first + 1) & (cap - 1);
	return tmp;
}

/**
 * @param q queue
 * @return  delete the first element from the queue and return its value.
 *          If queue is empty, result is undefined.
 */
#define queue_del_first(q)                                                  \
	(q)->elems[queue_inc_first(&(q)->first, (q)->cap)]

/**
 *  For each loop,
 *
 *  int *queue;
 *  queue_create(queue, 4);"
 *
 *  int elem;
 *  queue_foreach(queue, elem) {
 *      printf("Elem : %d \n, elem);
 *  }
 */
#define queue_foreach(q, elem)                                              \
	for (size_t _k = 1, _i = queue_first(q);                            \
	     _k && _i != queue_last(q);                                     \
	     _k = !_k, _i = queue_next(q, _i))                              \
		for ((elem) = (q)->elems[_i]; _k; _k = !_k)

//        (type, name)
queue_def(int, int);
queue_def(unsigned int, uint);
queue_def(long, long);
queue_def(unsigned long, ulong);
queue_def(unsigned long long, ull);
queue_def(uint32_t, 32);
queue_def(uint64_t, 64);
queue_def(double, double);
queue_def(const char *, str);
queue_def(void *, ptr);

#endif
