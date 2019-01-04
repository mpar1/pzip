#ifndef QUEUE_C_
#define QUEUE_C_

#include <stdio.h>
#include <errno.h>
#include <stdint.h> // portable int types
#include <stdlib.h> // exit
#include <pthread.h> // pthreads
// #include <semaphore.h> // semaphores

#define print_flow printf

void exit_if (int boolean, const char* msg);

typedef struct __node_t {
    void* data;
    char* next;
} node_t;

typedef struct __queue_t {
    node_t* head;
    node_t* tail;
    int64_t count;
    int64_t max_size;
    pthread_cond_t has_node;
    pthread_mutex_t lock;
    
    node_t* free_nodes;
    int64_t free_count;
    pthread_spinlock_t free_lock;
}

void init (queue_t* q, uint64_t initial_nodes);
void enqueue (queue_t* q, void* data);
void dequeue (queue_t* q, void** data);
void destroy (queue_t* q);
void exit_if (int boolean, const char* msg);


/*********************************************************
 *                          Main
 *********************************************************/

int main (int argc, const char* argv[])
{}

void init (queue_t* q, uint64_t initial_nodes)
{
    q->lock      = PTHREAD_MUTEX_INITIALIZER;
    q->has_node  = PTHREAD_COND_INITIALIZER;
    pthread_spin_init(&q->free_lock, PTHREAD_PROCESS_PRIVATE);
    
    q->head       = NULL;
    q->tail       = NULL;
    q->count      = 0;
    q->free_nodes = NULL;
    q->free_count = 0;

    for (uint64_t i = 0; i < initial_nodes; i++)
    {
        node_t* node = malloc(node_t);
        exit_if (node == NULL, "cannot allocate free node");
        node.next = free_nodes;
        q->free_nodes = node;
        q->free_count++;
    }
}

void enqueue (queue_t* q, void* data)
{
                print_flow ("Enqueue. Data: %p, \t Queue: %p\n", data, q);
                print_flow ("Enqueue. Acquiring the free lock\n");
    pthread_spin_lock (&q->free_lock);
                print_flow ("Enqueue. Acquired the free lock\n");
    node_t* node;
    if (q->free_count == 0)
    {
        node = malloc(sizeof(node_t));
        exit_if (node == NULL, "cannot allocate free node");
                print_flow ("Enqueue. No free node. Created one\n");
    }
    else
    {
        node = q->free_nodes;
        q->free_nodes = node->next;
        q->free_count--;
                print_flow ("Enqueue. Free node: %p\n", node);
    }
    pthread_spin_unlock (&q->free_lock);

    // modify data field of the free node
    node->data = data;
    node->next = NULL;

                print_flow ("Enqueue. Acquiring the data lock\n");
    // acquire tail lock
    pthread_lock (&q->lock);
                print_flow ("Enqueue. Acquired the data lock\n");
    // append the node to the tail
    if (q->count != 0)
    {   q->tail->next = node;   }
    else
    {   q->head = node; }
    q->tail = node;
    q->count++;
                print_flow ("Enqueue. Enqueued\n");
    // release tail lock
    pthread_unlock (&q->lock);
    pthread_cond_signal (&q->has_node);
                print_flow ("Enqueue. Signaled has_node\n");
}

void dequeue (queue_t* q, void** data)
{
                print_flow ("Dequeue. Queue: %p\n", q);
                print_flow ("Dequeue. Acquiring the head lock\n");
    // acquire head lock
    pthread_lock (&q->lock);
                print_flow ("Dequeue. Acquired the head lock\n");
    // wait for at least one node in queue
                print_flow ("Dequeue. Wait for at least one node in queue\n");
    while (q->count == 0)
    {   pthread_cond_wait (&q->has_node, &q->lock);   }
    // take the head off and move head forward
    node_t* node = q->head;
                print_flow ("Dequeue. Dequeue head: %p\n", node);
    q->head = q->head->next;
    q->count--;
    if (q->count == 0)
    {   q->tail = NULL;   }
    // release head lock
    pthread_unlock (&lock);

    *data = node->data;
    
    // acquire free lock
                print_flow ("Dequeue. Acquiring free lock\n");
    pthread_spin_lock (&q->free_lock);
    // cons the now freed node onto the free list
    node->next = q->free_nodes;
    q->free_nodes = node;
    // release free lock
    pthread_spin_unlock (&q->free_lock);
                print_flow ("Dequeue. Recycle free node: %p\n", node);
}

void destroy (queue_t* q)
{
    pthread_lock (&q->lock);
    node_t* node = q->head;
    while (node != NULL)
    {
        free (node);
        node = node->next;
    }
    pthread_unlock (&q->lock);
    pthread_mutex_destroy (&q->lock);
    pthread_cond_destroy (&q->has_node);

    pthread_spin_lock (&q->free_lock);
    node_t* node = q->free_nodes;
    while (node != NULL)
    {
        free (node);
        node = node->next;
    }
    pthread_spin_unlock (&q->free_lock);
    pthread_spin_destroy (&q->free_lock);
                print_flow ("Destroy. Destroyed queue: %p\n", q);
}

void exit_if (int boolean, const char* msg)
{
    if (boolean) {
        fprintf (stderr, "%s", msg);
        exit (1);
    }
}

#endif