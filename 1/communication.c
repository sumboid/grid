#include <mpi.h>
#include <pthread.h>
#include <unistd.h>

#include "types.h"
#include "global.h"

void mpi_send(int message, int tag, int to) {
    pthread_mutex_lock(&state->mcomm);
    MPI_Send(&message, 1, MPI_INT, to, tag, state->comm);
    pthread_mutex_unlock(&state->mcomm);
}

message_t mpi_recv(int tag, int from) {
    int have_message = 0;
    while(1) {
        pthread_mutex_lock(&state->mcomm);
        MPI_Iprobe(from, tag, state->comm, &have_message, MPI_STATUS_IGNORE);

        if(have_message) {
            int im;
            MPI_Status status;
            int end = MPI_Recv(&im, 1, MPI_INT, from, tag, state->comm, &status);
            pthread_mutex_unlock(&state->mcomm);
            if(end != MPI_SUCCESS) return (message_t) { .rank = -1, .m = -1 };
            return (message_t) { .rank = status.MPI_SOURCE, .m = (message) im };
        }

        pthread_mutex_unlock(&state->mcomm);
        sleep(1);
    }
}
