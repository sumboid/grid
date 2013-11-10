#pragma once
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "types.h"
#include "state.h"

typedef struct {
    char* all;
    size_t size;
    pthread_mutex_t mutex;
} compl_t;

static compl_t* compl_init(mpi_state_t* state) {
    compl_t* c = malloc(sizeof(compl_t));
    c->size = state->size;
    c->all = calloc(c->size, sizeof(char));
    pthread_mutex_init(&c->mutex, NULL);

    return c;
}

static int compl_check(compl_t* c) {
    int result = 1;

    pthread_mutex_lock(&c->mutex);
    for(int i = 0; i < c->size; ++i)
        if(c->all[i] == 0) {
            result = 0;
            break;
        }
    pthread_mutex_unlock(&c->mutex);
    return result;
}

static void compl_set(compl_t* c, size_t index, char value) {
    pthread_mutex_lock(&c->mutex);
    c->all[index] = value;
    pthread_mutex_unlock(&c->mutex);
}

static void compl_resize(compl_t* c, mpi_state_t* state) {
    size_t old_size = c->size;
    pthread_mutex_lock(&c->mutex);
    c->size = state->size;
    c->all = realloc(c->all, c->size * sizeof(char));
    memset(c->all + old_size, 0, c->size - old_size);
    pthread_mutex_unlock(&c->mutex);
}

static void compl_print(compl_t* c) {
    for(int i = 0; i < c->size; ++i) {
        printf("%d -> %d\n", i, c->all[i]);
    }
}


