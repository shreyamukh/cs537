#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include "ring_buffer.h"
#include "common.h"

typedef struct node {
    key_type key;
    value_type value;
    struct node *next;
} node;

typedef struct bucket {
    node *head;
    pthread_mutex_t mutex_lock;  // Mutex to control access to this bucket
} bucket;

typedef struct hashTable {
    bucket *buckets;
    index_t size;
} hashTable;

hashTable *hashtable;

// Creates a new node with the given key and value
node *createNode(key_type key, value_type value) {
    node *new_node = malloc(sizeof(node));
    if (!new_node) {
        error("ERROR : malloc failed");
        exit(EXIT_FAILURE);
    }
    new_node->key = key;
    new_node->value = value;
    new_node->next = NULL;
    return new_node;
}

// Initializes the hash table with a specified number of buckets
hashTable *initializeHashtable(index_t size) {
    hashTable *table = malloc(sizeof(hashTable));
    if (!table) {
        error("ERROR : malloc failed");
        exit(EXIT_FAILURE);
    }

    table->size = size;
    table->buckets = malloc(table->size * sizeof(bucket));
    if (!table->buckets) {
        free(table);
        error("ERROR : malloc failed");
        exit(EXIT_FAILURE);
    }

    for (index_t i = 0; i < table->size; i++) {
        table->buckets[i].head = NULL;
        pthread_mutex_init(&table->buckets[i].mutex_lock, NULL);
    }
    return table;
}

// Adds or updates a node in the hash table for a given key and value
void put(key_type key, value_type value) {
    index_t index = hash_function(key, hashtable->size);
    bucket *bucket = &hashtable->buckets[index];

    pthread_mutex_lock(&bucket->mutex_lock);
    node *prev_node = NULL;
    node *iter = bucket->head;

    while (iter) {
        if (iter->key == key) {
            iter->value = value;
            pthread_mutex_unlock(&bucket->mutex_lock);
            return;
        }
        prev_node = iter;
        iter = iter->next;
    }

    node *new_node = createNode(key, value);
    if (prev_node) {
        prev_node->next = new_node;
    } else {
        bucket->head = new_node;
    }
    pthread_mutex_unlock(&bucket->mutex_lock);
}

// Retrieves a value by its key from the hash table
value_type get(key_type key) {
    index_t index = hash_function(key, hashtable->size);
    bucket *bucket = &hashtable->buckets[index];

    pthread_mutex_lock(&bucket->mutex_lock);
    for (node *iter = bucket->head; iter != NULL; iter = iter->next) {
        if (iter->key == key) {
            pthread_mutex_unlock(&bucket->mutex_lock);
            return iter->value;
        }
    }
    pthread_mutex_unlock(&bucket->mutex_lock);
    return 0;  // Return a default value if not found
}

// Server thread to handle incoming PUT and GET requests via shared memory
void *serverThreadHandler(void *arg) {
    int fd = open("shmem_file", O_RDWR);
    if (fd < 0) {
        error("ERROR : could not open shared memory file");
        exit(EXIT_FAILURE);
    }

    struct stat stat_file;
    if (fstat(fd, &stat_file) == -1) {
        error("ERROR : could not stat file");
        exit(EXIT_FAILURE);
    }

    void *shm_base = mmap(NULL, stat_file.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_base == MAP_FAILED) {
        error("ERROR : mmap failed");
        exit(EXIT_FAILURE);
    }

    close(fd);
    struct ring *ring = (struct ring *)shm_base;

    struct buffer_descriptor bd;
    while (1) {
        ring_get(ring, &bd);
        if (bd.req_type == PUT) {
            put(bd.k, bd.v);
        } else if (bd.req_type == GET) {
            bd.v = get(bd.k);
        }
        struct buffer_descriptor *res = (struct buffer_descriptor *)((char *)shm_base + bd.res_off);
        memcpy(res, &bd, sizeof(struct buffer_descriptor));
        res->ready = 1;
    }
}

// Main method to initialize and start server threads
int main(int argc, char *argv[]) {
    int numberServerThreads, size, choice;

    while ((choice = getopt(argc, argv, "n:s:")) != -1) {
        switch (choice) {
            case 'n':
                numberServerThreads = atoi(optarg);
                break;
            case 's':
                size = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-n numberServerThreads] [-s initial_hashtable_size]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    hashtable = initializeHashtable(size);
    pthread_t threads[numberServerThreads];

    for (int i = 0; i < numberServerThreads; i++) {
        if (pthread_create(&threads[i], NULL, serverThreadHandler, NULL) != 0) {
            error("ERROR : Could not create server thread");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < numberServerThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
