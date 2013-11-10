#include <mpi.h>
#include <pthread.h>
#include <unistd.h>

#include "types.h"
#include "global.h"
#include "state.h"
#include "communication.h"

void mpi_send(int message, int tag, int to) {
    lock_state(state);
    MPI_Send(&message, 1, MPI_INT, to, tag, state->comm);
    unlock_state(state);
}

void mpi_sync_send(int message, int tag, int to) {
    lock_state(state);
    MPI_Ssend(&message, 1, MPI_INT, to, tag, state->comm);
    unlock_state(state);
}

message_t mpi_recv(int tag, int from) {
    int have_message = 0;
    while(1) {
        lock_state(state);
        MPI_Iprobe(from, tag, state->comm, &have_message, MPI_STATUS_IGNORE);

        if(have_message) {
            int im;
            MPI_Status status;
            int end = MPI_Recv(&im, 1, MPI_INT, from, tag, state->comm, &status);
            unlock_state(state);
            if(end != MPI_SUCCESS) return (message_t) { .rank = -1, .m = -1 };
            return (message_t) { .rank = status.MPI_SOURCE, .m = (message) im };
        }

        unlock_state(state);

        pthread_mutex_lock(&mend);
        if(end) {
            pthread_mutex_unlock(&mend);
            return (message_t) { .rank = 0, .m = END };
        }
        pthread_mutex_unlock(&mend);
        sleep(1);
    }
}

void bcast(message m) {
    for(int i = 0; i < state->size; ++i) {
        if(state->rank == i) continue;
        mpi_send(m, MESSAGE, i);
    }
}

message_t recv_message() {
    return mpi_recv(MESSAGE, MPI_ANY_SOURCE);
}

void send_message(int rank, message m) {
    mpi_send(m, MESSAGE, rank);
}

void send_fragments(int rank, int fragments) {
    mpi_send(fragments, JOB, rank);
}

int recv_fragments(int rank) {
    return mpi_recv(JOB, rank).m;
}

