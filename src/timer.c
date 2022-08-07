#include "timer.h"
#include "time.h"

void* timeit(void* data) {
    struct timer* timer = (struct timer*) data;
    time_sleep(timer->ms);
    timer->callback(timer->data);
}

void timer_init(struct timer* timer) {
    timer = (struct timer*) malloc(sizeof(struct timer));
    thread_init(&timer->th);
}

void timer_start(struct timer* timer, int ms, void* (*callback) (void*), void* data) {
    timer->ms = ms;
    timer->callback = callback;
    timer->data = data;
    thread_start(&timer->th, timeit, timer);
}

void timer_term(struct timer* timer) {
    thread_term(&timer->th);
    free(timer);
}