#include "thread.h"

struct timer {
    struct thread th;
    int ms;
    void* (*callback) (void*);
    void* data;
};

void timer_init(struct timer* timer);
void timer_start(struct timer* timer, int ms, void* (*callback) (void*), void* data);
void timer_term(struct timer* timer);