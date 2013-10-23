#include <stdio.h>
#include <pthread.h>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_NAME "titanic"
#define PORT_FILE "current-port"

#define START_THREAD(x) pthread_t x##_thread; pthread_create(&x##_thread, NULL, x, NULL)

typedef enum {
	CONNECT,
	GET_PORT
} message;

typedef struct {
	MPI_Comm comm;
	int rank;
	int size;
	int worker;
} mpi_state_t;

mpi_state_t* state = NULL;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

char mpi_port[MPI_MAX_PORT_NAME];
FILE* port_file = NULL;

static void init_state(int argc, char** argv);
static void connect();
static MPI_Comm merge(MPI_Comm comm);
static void* listener();
static void update_state(MPI_Comm comm, int new_comm, int worker, int new_worker);
static int port_file_exist();
static void publish_name();
static void unpublish_name();
static void republish_name();
static void bcast(message);
static void read_port();
static void setup();


int main(int argc, char *argv[])
{
	init_state(argc, argv);
	setup();

	START_THREAD(listener);
	pthread_exit(&listener_thread);
	printf("Bye!\n");
	MPI_Finalize();
	return 0;
}

static void init_state(int argc, char** argv) {
	int required = MPI_THREAD_MULTIPLE;
	int provided = 0;
	if(MPI_SUCCESS != MPI_Init_thread(&argc, &argv, required, &provided)) {
		printf(":(\n"); fflush(stdout);
	}

	state = malloc(sizeof(mpi_state_t));
	update_state(MPI_COMM_WORLD, 1, 0, 1);
}

static void setup() {
	if(state->rank == 0) {
		if(!port_file_exist()) {
			publish_name(); 
			bcast(GET_PORT);
			update_state(MPI_COMM_NULL, 0, 1, 1);
		}
		else {
			bcast(CONNECT);
			connect();	
		}
	} else {
		int behaviour;
		MPI_Recv(&behaviour, 1, MPI_INTEGER,
						 0, 0, state->comm, NULL);

		if((message)behaviour == CONNECT) connect();
		else if((message)behaviour == GET_PORT) { read_port(); update_state(MPI_COMM_NULL, 0, 1, 1); }
	}	
}

static void bcast(message m) {
	int im = m;
	for(int i = 0; i < state->size; ++i) {
		if(state->rank == i) continue;
		MPI_Send(&m, 1, MPI_INTEGER,
						 i, 0, state->comm);
		}
}

static void connect() {
	MPI_Comm newcomm;
	read_port();
	MPI_Comm_connect(mpi_port, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &newcomm);
	update_state(newcomm, 1, 0, 0);
	
	MPI_Comm newcom = merge(state->comm);
	update_state(newcom, 1, 1, 1);
}

static MPI_Comm merge(MPI_Comm comm) {
	MPI_Comm newcomm;
	MPI_Intercomm_merge(comm, (state->worker - 1) == -1 ? 1 : 0, &newcomm);
	return newcomm;
}

static void* listener() {
	while(1) {
		MPI_Comm newcomm;
		MPI_Comm tmpcomm;
		MPI_Comm_accept(mpi_port, MPI_INFO_NULL, 0, state->comm, &tmpcomm);
		newcomm = merge(tmpcomm);
		if(state->rank == 0) republish_name();
		// Stop all shit
		//if(&state->comm != MPI_COMM_WORLD) MPI_Comm_free(&state->comm);
		update_state(newcomm, 1, 0, 0);
		printf("----------\nSuccessfully merged!\nrank = %d\nsize = %d\n", state->rank, state->size);
		// Resume all shit	
	}
}

static void read_port() {
	if(port_file == NULL) port_file = fopen(PORT_FILE, "r");
	fread(mpi_port, sizeof(char), MPI_MAX_PORT_NAME, port_file);
}

static int port_file_exist() {
	return access(PORT_FILE, F_OK) != -1;
}

static void publish_name() { 
	MPI_Open_port(MPI_INFO_NULL, mpi_port);
	if(MPI_SUCCESS != MPI_Publish_name(SERVER_NAME, MPI_INFO_NULL, mpi_port)) {
		printf("ohshi\n"); fflush(stdout);
	}
	
	if(port_file == NULL) port_file = fopen(PORT_FILE, "w");
	fwrite(mpi_port, sizeof(char), MPI_MAX_PORT_NAME, port_file);
	rewind(port_file);
}

static void unpublish_name() {
	MPI_Unpublish_name(SERVER_NAME, MPI_INFO_NULL, mpi_port);
}

static void republish_name() {
	unpublish_name();
	publish_name();
}

static void update_state(MPI_Comm comm, int new_comm, int worker, int new_worker) {
	pthread_mutex_lock(&state_mutex);
	if(new_comm) {
		state->comm = comm;
		MPI_Comm_rank(comm, &state->rank);
		MPI_Comm_size(comm, &state->size);	
	}
	if(new_worker) state->worker = worker;
	pthread_mutex_unlock(&state_mutex);
}


