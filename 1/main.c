#include <stdio.h>
#include <pthread.h>
#include <mpi.h>
#include <stdlib.h>

#define SERVER_NAME "Titanic"
#define PORT_FILENAME "current-port"
#define PRINT_PROCESS_INFO(x) (printf("Size: %d, Rank: %d\n", x.size, x.rank))

typedef struct {
	MPI_Comm comm;
	int rank;
	int size;
	int worker;
} mpi_state_t;

int main(int argc, char *argv[])
{
	mpi_state_t* state = init_mpi();
	
	char* port = lookup();
	if(port != NULL) {
		connect(port, state);
		free(port);
		merge(state);
	}
	state->worker = 1;

	pthread_t listener_thread;
	pthread_create(&listener_thread, NULL, listener, NULL);

	return 0;
}

mpi_state_t* init_mpi() {
	int required = MPI_THREAD_MULTIPLE;
	int provided = 0;
	MPI_Init_thread(&argc, &argv, required, &provided);

	mpi_state_t* state = malloc(sizeof(mpi_state_t));
	state->comm = MPI_COMM_NULL;
	MPI_Comm_rank(MPI_COMM_WORLD, &state.rank);
	MPI_Comm_size(MPI_COMM_WORLD, &state.size);	
	state->worker = 0;
}

char* lookup() {
	char* port = calloc(MPI_MAX_PORT_NAME, sizeof(char));
	if(MPI_SUCCESS != MPI_Lookup_name(SERVER_NAME, MPI_INFO_NULL, port)) {
		free(port);
		return NULL;
	}
	return port;
}

void connect(char* port, mpi_state_t* state) {
	MPI_Comm_connect(port, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &state->comm);
}

void merge(mpi_state_t* state) {
	MPI_Comm oldcomm = state->comm;
	MPI_Comm_merge(oldcom, state->worker, &state->comm);
	MPI_Comm_free(&oldcom);

	MPI_Comm_rank(state->comm, &state->rank);
	MPI_Comm_size(state->comm, &state->rank);
}

void* listener() {

}

