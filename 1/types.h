#pragma once
#include <pthread.h>

typedef enum {
	CONNECT,
	GET_PORT,
	WANNA_SLEEP,
	INCOMING
} message;

enum {
	MESSAGE,
	JOB
};

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

  pthread_mutex_t mcomm;
} mpi_state_t;
