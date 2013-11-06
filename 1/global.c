#include <mpi.h>
#include <pthread.h>
#include <stdio.h>

#include "types.h"

mpi_state_t* state;

char mpi_port[MPI_MAX_PORT_NAME];
FILE* port_file = NULL;

pthread_mutex_t sleep_mutex = PTHREAD_MUTEX_INITIALIZER;
int sleeptime = 0;
int sleepfragment = 1;
int empty = 0;
int wannasleep_sended = 0;
