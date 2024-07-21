#include "ring_buffer.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

// Mutexes for separate operations
pthread_mutex_t ring_submit_lock;
pthread_mutex_t ring_get_lock;

/*
 * Initialize the ring
 * @param r A pointer to the ring
 * @return 0 on success, negative otherwise - this negative value will be
 * printed to output by the client program
*/
int init_ring(struct ring *r) {

    // Initialize ring buffer submit/get locks
    if (pthread_mutex_init(&ring_submit_lock, NULL) != 0 || pthread_mutex_init(&ring_get_lock, NULL) != 0) {
        exit(EXIT_FAILURE);
    }

    // Initialize producer and consumer head and tail
    r->p_head = 0;
    r->p_tail = 0;

    // Initialize buffer
    for (int i = 0; i < RING_SIZE; i++) {
        r->buffer[i].ready = 0;
    }

    return 0;
}

/*
 * Submit a new item - should be thread-safe
 * This call will block the calling thread if there's not enough space
 * @param r The shared ring
 * @param bd A pointer to a valid buffer_descriptor - This pointer is only
 * guaranteed to be valid during the invocation of the function
*/
void ring_submit(struct ring *r, struct buffer_descriptor *bd) {

    // Acquire lock
    pthread_mutex_lock(&ring_submit_lock);

    // Wait to get place in buffer
    while ((r->p_head + 1) % RING_SIZE == r->p_tail) {
        // Unlock to prevent deadlock and yield processor
        pthread_mutex_unlock(&ring_submit_lock);
        sched_yield();

        // Re-acquire lock
        pthread_mutex_lock(&ring_submit_lock);
    }

    // Copy bd to ring buffer and set ready flag
    memcpy(&r->buffer[r->p_head], bd, sizeof(struct buffer_descriptor));
    r->buffer[r->p_head].ready = 1;

    // Update producer head
    r->p_head = (r->p_head + 1) % RING_SIZE;

    // Let go of lock
    pthread_mutex_unlock(&ring_submit_lock);
}

/*
 * Get an item from the ring - should be thread-safe
 * This call will block the calling thread if the ring is empty
 * @param r A pointer to the shared ring
 * @param bd pointer to a valid buffer_descriptor to copy the data to
 * Note: This function is not used in the clinet program, so you can change
 * the signature.
*/
void ring_get(struct ring *r, struct buffer_descriptor *bd) {

    // Acquire lock
    pthread_mutex_lock(&ring_get_lock);

    // Wait for data to be available
    while (r->p_tail == r->p_head || r->buffer[r->p_tail].ready != 1) {
        // Unlock to prevent deadlock and yield processor
        pthread_mutex_unlock(&ring_get_lock);
        sched_yield();

        // Re-acquire lock
        pthread_mutex_lock(&ring_get_lock);
    }
    // Copy bd from the ring buffer and reset ready flag
    memcpy(bd, &r->buffer[r->p_tail], sizeof(struct buffer_descriptor));
    r->buffer[r->p_tail].ready = 0;

    // Update producer tail
    r->p_tail = (r->p_tail + 1) % RING_SIZE;

    // Let go of lock
    pthread_mutex_unlock(&ring_get_lock);
}

// Clean up resources used by mutexes
void cleanup_ring() {
    pthread_mutex_destroy(&ring_submit_lock);
    pthread_mutex_destroy(&ring_get_lock);
}
