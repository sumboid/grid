#pragma once
#include <mpi.h>
#include <pthread.h>

typedef struct {
    MPI_Comm comm;

    int rank;
    int size;
    int oldsize;

    pthread_rwlock_t mcomm;
} mpi_state_t;

mpi_state_t* create_state();
void update_state(mpi_state_t* state, MPI_Comm comm);
static inline void lock_state(mpi_state_t* state) {
    pthread_rwlock_rdlock(&state->mcomm);
}
static inline void unlock_state(mpi_state_t* state) {
    pthread_rwlock_unlock(&state->mcomm);
}

int get_rank(mpi_state_t* state);
int get_size(mpi_state_t* state);
int get_oldsize(mpi_state_t* state);
MPI_Comm get_comm(mpi_state_t* state);
