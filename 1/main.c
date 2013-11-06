#include <stdio.h>
#include <pthread.h>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>

#include "types.h"
#include "helpers.h"
#include "config.h"
#include "global.h"
#include "communication.h"

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
static void send_fragments(int rank, int fragments);
static void get_time(int rank);
static int recv_fragments(int rank);
static void change_sleeptime(int size); 

int main(int argc, char *argv[])
{
	init_state(argc, argv);
	setup();
	START_THREAD(listener);
	START_THREAD(balancer);

	while(1) {
		if(sleeptime == 0) {
			
			if(!wannasleep_sended) { bcast(WANNA_SLEEP); wannasleep_sended = 1; }
			if(empty == state->size - 1) break; //Nothing to do
			sleep(1);
			continue;
		}
		printf("%d: Zzz\n", state->rank); fflush(stdout);
		wannasleep_sended = 0;

		change_sleeptime(-1);
		sleep(sleepfragment);
	}

	//printf("Ready! Please send SIGKILL."); fflush(stdout);
	MPI_Barrier(state->comm);
	MPI_Abort(state->comm, 359);
	WAIT_THREAD(listener);
	WAIT_THREAD(balancer);
	MPI_Finalize();
	return 0;
}

static void init_state(int argc, char** argv) {
	int required = MPI_THREAD_MULTIPLE;
	int provided = 0;
	if(MPI_SUCCESS != MPI_Init_thread(&argc, &argv, required, &provided)) {
		printf(":(\n"); fflush(stdout);
	}
	if(provided != required) { printf("Provided = %d\n", provided); exit(1); }

	state = calloc(1, sizeof(mpi_state_t));
  pthread_mutex_init(&state->mcomm, NULL);
	update_state(MPI_COMM_WORLD, 1, 0, 1, 0, 0);
}

static void setup() {
	if(state->rank == 0) {
		WRITE_MESSAGE_TABLE;
		if(!port_file_exist()) {
			sleeptime = 5 * MINUTE;
			publish_name(); 
			bcast(GET_PORT);
			MPI_Barrier(state->comm);
			update_state(MPI_COMM_NULL, 0, 1, 1, 1, 1);
		}
		else {
			bcast(CONNECT);
			MPI_Barrier(state->comm);
			connect();	
		}
	} else {
		message m = recv_message().m;
		MPI_Barrier(state->comm);
		if(m == CONNECT) {  connect(); }
		else if(m == GET_PORT) {  read_port(); update_state(MPI_COMM_WORLD, 0, 1, 1, 0, 0); }
	}
}

static void bcast(message m) {
	for(int i = 0; i < state->size; ++i) {
		if(state->rank == i) continue;
    mpi_send(m, MESSAGE, i);
  }
}

static message_t recv_message() {
	MPI_Status status;
	int im;
  return mpi_recv(MESSAGE, MPI_ANY_SOURCE);
}

static void connect() {
	MPI_Comm newcomm;
	read_port();
	MPI_Comm_connect(mpi_port, MPI_INFO_NULL, 0, state->comm, &newcomm);
	newcomm = merge(newcomm);
	update_state(newcomm, 1, 1, 1, 0, 0);
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

		int end = MPI_Comm_accept(mpi_port, MPI_INFO_NULL, 0, state->comm, &tmpcomm);
		if(end != MPI_SUCCESS) pthread_exit(NULL);

		newcomm = merge(tmpcomm);
		if(state->rank == 0) republish_name();
		update_state(newcomm, 1, 0, 0, 0, 0);
		printf("%d: size = %d\n", state->rank, state->size);
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
	if(update_comm != 0) {
	  pthread_mutex_lock(&state->mcomm);

		state->comm = comm;
		if(state->size == 0) state->oldsize = 1;
		else state->oldsize = state->size;
		MPI_Comm_rank(comm, &state->rank);
		MPI_Comm_size(comm, &state->size);	

    pthread_mutex_unlock(&state->mcomm);
	}
	if(update_worker != 0) state->worker = worker;
	if(update_can_give != 0) state->can_give = can_give;
}

static void* balancer() {
	while(1) {
		//printf("%d: Waiting for new message.\n", state->rank); fflush(stdout);
		message_t mes = recv_message();
		if(mes.rank == -1) pthread_exit(NULL);
		//printf("%d: Recieved some message. \n", state->rank); fflush(stdout);
		switch(mes.m) {
			case WANNA_SLEEP: give_time(mes.rank); break;
			case INCOMING: get_time(mes.rank); break;
			default: printf("%d: not cool: %d\n", state->rank, mes.m); fflush(stdout); break;
		}
	}
}

static void give_time(int rank) {
	printf("%d: Recieved WANNA_SLEEP message from %d\n", state->rank, rank); fflush(stdout);
	//if(!state->can_give) return;
	int fragments = (sleeptime / sleepfragment) / (state->size - state->oldsize + 1);
	if(fragments == 0) {
		printf("%d: Can't give smth! I have only %d fragments!\n", state->rank, sleeptime / sleepfragment); fflush(stdout);
		empty++;
		return;
	}
	
	empty = 0;	
	//printf("%d: Send INCOMING to %d \n", state->rank, rank); fflush(stdout);
	send_message(rank, INCOMING);
	send_fragments(rank, fragments);
}

static void send_message(int rank, message m) {
    mpi_send(m, MESSAGE, rank);
}

static void send_fragments(int rank, int fragments) {
	change_sleeptime(-fragments);
	//printf("%d: Start sending fragments to %d\n", state->rank, rank); fflush(stdout);
  mpi_send(fragments, JOB, rank);
	//printf("%d: Fragments sended to %d\n", state->rank, rank); fflush(stdout);
}

static void get_time(int rank) {
	//printf("%d: Recieved INCOMING message\n", state->rank); fflush(stdout);
	int fragments = recv_fragments(rank);
	//printf("%d: Get %d fragments\n", state->rank, fragments); fflush(stdout);
	change_sleeptime(fragments);
}

static int recv_fragments(int rank) {
	//printf("%d: Get fragments from %d\n", state->rank, rank); fflush(stdout);
  return mpi_recv(JOB, rank).m;
}

static void change_sleeptime(int size) {
	pthread_mutex_lock(&sleep_mutex);
	//printf("%d: Added %d time\n", state->rank, size * sleepfragment); fflush(stdout);
	sleeptime += size * sleepfragment;
	pthread_mutex_unlock(&sleep_mutex);
}

