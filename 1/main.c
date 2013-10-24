#include <stdio.h>
#include <pthread.h>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_NAME "titanic"
#define PORT_FILE "current-port"
#define MINUTE 60
#define START_THREAD(x) pthread_t x##_thread; pthread_create(&x##_thread, NULL, x, NULL)

typedef enum {
	CONNECT,
	GET_PORT,
	WANNA_SLEEP,
	INCOMING
} message;

typedef struct {
	int rank;
	message m;
} message_t;

typedef struct {
	MPI_Comm comm;
	int rank;
	int size;
	int oldsize;
	int worker;
	int can_give;
} mpi_state_t;

mpi_state_t* state = NULL;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

char mpi_port[MPI_MAX_PORT_NAME];
FILE* port_file = NULL;

int sleeptime = 0;
int sleepfragment = 1;
pthread_mutex_t sleep_mutex = PTHREAD_MUTEX_INITIALIZER;
int empty = 0;
int wannasleep_sended = 0;

static void init_state(int argc, char** argv);
static void connect();
static MPI_Comm merge(MPI_Comm comm);
static void* listener();
static void update_state(MPI_Comm comm, int update_comm, 
						 int worker, int update_worker,
						 int can_give, int update_can_give);
static int port_file_exist();
static void publish_name();
static void unpublish_name();
static void republish_name();
static void bcast(message);
static void read_port();
static void setup();
static message_t recv_message();
static void* balancer();
static void give_time(int rank); 
static void send_message(int rank, message m);
static void send_time(int rank, int fragments);
static void get_time(int rank);
static int recv_fragments(int rank);
static void change_sleeptime(int size); 

int main(int argc, char *argv[])
{
	//printf("%d: I'm here!\n", state->rank); fflush(stdout);
	init_state(argc, argv);
	printf("%d: Init ready!\n", state->rank); fflush(stdout);
	if(state->rank == 0) sleeptime = 1 * MINUTE;

	setup();
	printf("%d: Setup ready!\n", state->rank); fflush(stdout);

	START_THREAD(listener);
	START_THREAD(balancer);
	pthread_detach(listener_thread);
	pthread_detach(listener_thread);
	// while(1) {
	// 	if(sleeptime == 0) {
	// 		//printf("%d: I wanna sleep!\n", state->rank); fflush(stdout);
	// 		if(!wannasleep_sended) { bcast(WANNA_SLEEP); wannasleep_sended = 1; }
	// 		if(empty == state->size - 1) break; //Nothing to do
	// 		sleep(1);
	// 		continue;
	// 	}
	// 	wannasleep_sended = 0;

	// 	change_sleeptime(-1);
	// 	sleep(sleepfragment);
	// }
	sleep(60);
	printf("Ready! Please send SIGKILL."); fflush(stdout);
	MPI_Finalize();
	return 0;
}

static void init_state(int argc, char** argv) {
	int required = MPI_THREAD_MULTIPLE;
	int provided = 0;
	if(MPI_SUCCESS != MPI_Init_thread(&argc, &argv, required, &provided)) {
		printf(":(\n"); fflush(stdout);
	}

	state = calloc(1, sizeof(mpi_state_t));
	update_state(MPI_COMM_WORLD, 1, 0, 1, 0, 0);
}

static void setup() {
	if(state->rank == 0) {
		if(!port_file_exist()) {
			publish_name(); 
			bcast(GET_PORT);
			update_state(MPI_COMM_NULL, 0, 1, 1, 1, 1);
		}
		else {
			bcast(CONNECT);
			connect();	
		}
	} else {
		message m = recv_message().m;
		if(m == CONNECT) connect();
		else if(m == GET_PORT) { read_port(); update_state(MPI_COMM_NULL, 0, 1, 1, 0, 0); }
	}	
}

static void bcast(message m) {
	int im = m;
	for(int i = 0; i < state->size; ++i) {
		if(state->rank == i) continue;
		MPI_Ssend(&m, 1, MPI_INTEGER,
						 i, state->rank, state->comm);
		}
}

static message_t recv_message() {
	MPI_Status status;
	int im;
	MPI_Recv(&im, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, state->comm, &status);
	return (message_t) { .rank = status.MPI_SOURCE, .m = (message) im };
}

static void connect() {
	MPI_Comm newcomm;
	read_port();
	MPI_Comm_connect(mpi_port, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &newcomm);
	update_state(newcomm, 1, 0, 0, 0, 0);
	
	MPI_Comm newcom = merge(state->comm);
	update_state(newcom, 1, 1, 1, 0, 0);
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
		update_state(MPI_COMM_NULL, 0, 0, 0, 1, 1);
		MPI_Comm_accept(mpi_port, MPI_INFO_NULL, 0, state->comm, &tmpcomm);
		newcomm = merge(tmpcomm);
		if(state->rank == 0) republish_name();
		update_state(newcomm, 1, 0, 0, 0, 0);
		printf("----------\nSuccessfully merged!\nrank = %d\nsize = %d\n", state->rank, state->size);
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
	MPI_Publish_name(SERVER_NAME, MPI_INFO_NULL, mpi_port);
	
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

static void update_state(MPI_Comm comm, int update_comm, 
						 int worker, int update_worker,
						 int can_give, int update_can_give) {
	pthread_mutex_lock(&state_mutex);
	if(update_comm) {
		state->comm = comm;
		if(state->size == 0) state->oldsize = 1;
		else state->oldsize = state->size;
		MPI_Comm_rank(comm, &state->rank);
		MPI_Comm_size(comm, &state->size);	
	}
	if(update_worker) state->worker = worker;
	if(update_can_give) state->can_give = can_give;
	pthread_mutex_unlock(&state_mutex);
}

static void* balancer() {
	while(1) {
		printf("%d: Waiting for new message.\n", state->rank); fflush(stdout);
		message_t mes = recv_message();
		switch(mes.m) {
			case WANNA_SLEEP: give_time(mes.rank); break;
			case INCOMING: get_time(mes.rank); break;
			default: printf("not cool\n"); break;
		}
	}
}

static void give_time(int rank) {
	printf("%d: Recieved WANNA_SLEEP message\n", state->rank); fflush(stdout);
	if(!state->can_give) return;
	if(sleeptime == 0) {
		empty++;
		return;
	}
	
	empty = 0;
	int fragments = (sleeptime / sleepfragment) * state->oldsize / state->size;
	
	send_message(rank, INCOMING);
	send_time(rank, fragments);
}

static void send_message(int rank, message m) {
	int im = m;
	MPI_Ssend(&im, 1, MPI_INTEGER, rank, state->rank, state->comm);
}

static void send_time(int rank, int fragments) {
	change_sleeptime(-fragments);
	MPI_Ssend(&fragments, 1, MPI_INTEGER, rank, state->rank, state->comm);
}

static void get_time(int rank) {
	printf("%d: Recieved INCOMING message\n", state->rank); fflush(stdout);
	int fragments = recv_fragments(rank);
	change_sleeptime(fragments);
}

static int recv_fragments(int rank) {
	int f;
	MPI_Recv(&f, 1, MPI_INT, rank, rank, state->comm, NULL);
	return f;
}

static void change_sleeptime(int size) {
	pthread_mutex_lock(&sleep_mutex);
	printf("%d: Added %d time\n", state->rank, size * sleepfragment); fflush(stdout);
	sleeptime += size * sleepfragment;
	pthread_mutex_unlock(&sleep_mutex);
}

