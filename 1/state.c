#include "state.h"
#include <pthread.h>
#include <mpi.h>
#include <stdlib.h>

mpi_state_t* create_state() {
    mpi_state_t* state = calloc(1, sizeof(mpi_state_t));
    pthread_rwlock_init(&state->mcomm, NULL);
    state->size = 1;
    update_state(state, MPI_COMM_WORLD);
    return state;
}

void update_state(mpi_state_t* state, MPI_Comm comm) {
    pthread_rwlock_wrlock(&state->mcomm);
    state->comm = comm;
    state->oldsize = state->size;
    MPI_Comm_rank(comm, &state->rank);
    MPI_Comm_size(comm, &state->size);
    pthread_rwlock_unlock(&state->mcomm);
}

int get_rank(mpi_state_t* state) {
    int r;
    lock_state(state);
    r = state->rank;
    unlock_state(state);
    return r;
}

int get_size(mpi_state_t* state) {
    int r;
    lock_state(state);
    r = state->size;
    unlock_state(state);
    return r;
}

int get_oldsize(mpi_state_t* state) {
    int r;
    lock_state(state);
    r = state->oldsize;
    unlock_state(state);
    return r;
}

MPI_Comm get_comm(mpi_state_t* state) {
    MPI_Comm r;
    lock_state(state);
    r = state->comm;
    unlock_state(state);
    return r;
}
