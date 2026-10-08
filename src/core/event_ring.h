#pragma once
/* Single-producer / single-consumer ring of GrainEvents. C only (uses <stdatomic.h>, which C++17 lacks),
 * so it is included by the engine's .c files and never by granular_engine.h.
 * Producer = audio thread (ger_push), consumer = GUI thread (ger_pop). Never blocks, never allocates;
 * when full the NEW event is dropped and counted. */
#include <stdatomic.h>
#include "grain_event.h"

#define GE_EVENT_RING_SIZE 512   /* power of two */

typedef struct GrainEventRing {
    GrainEvent buf[GE_EVENT_RING_SIZE];
    atomic_uint head;      /* written only by the producer */
    atomic_uint tail;      /* written only by the consumer */
    atomic_uint dropped;   /* events lost to a full ring */
} GrainEventRing;

static inline void ger_init(GrainEventRing* r){
    atomic_init(&r->head,0u); atomic_init(&r->tail,0u); atomic_init(&r->dropped,0u);
}
static inline int ger_push(GrainEventRing* r, GrainEvent e){
    unsigned h=atomic_load_explicit(&r->head,memory_order_relaxed);
    unsigned t=atomic_load_explicit(&r->tail,memory_order_acquire);
    if(h-t>=GE_EVENT_RING_SIZE){ atomic_fetch_add_explicit(&r->dropped,1u,memory_order_relaxed); return 0; }
    r->buf[h&(GE_EVENT_RING_SIZE-1)]=e;
    atomic_store_explicit(&r->head,h+1u,memory_order_release);
    return 1;
}
static inline int ger_pop(GrainEventRing* r, GrainEvent* out){
    unsigned t=atomic_load_explicit(&r->tail,memory_order_relaxed);
    unsigned h=atomic_load_explicit(&r->head,memory_order_acquire);
    if(t==h) return 0;
    *out=r->buf[t&(GE_EVENT_RING_SIZE-1)];
    atomic_store_explicit(&r->tail,t+1u,memory_order_release);
    return 1;
}
