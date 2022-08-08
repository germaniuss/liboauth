#include "thread.h"
#include <stdbool.h>

struct timer {
    struct thread th;
    bool running;
    bool init;
    int ms;
    void* (*callback) (void*);
    void* data;
};

void timer_init(struct timer* timer);
void timer_start(struct timer* timer, int ms, void* (*callback) (void*), void* data);
void timer_term(struct timer* timer);