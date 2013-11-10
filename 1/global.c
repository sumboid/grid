#include <mpi.h>
#include <pthread.h>
#include <stdio.h>

#include "types.h"
#include "compl.h"
#include "state.h"

mpi_state_t* state;

char mpi_port[MPI_MAX_PORT_NAME];
FILE* port_file = NULL;

pthread_mutex_t sleep_mutex = PTHREAD_MUTEX_INITIALIZER;
int sleeptime = 0;
int sleepfragment = 1;
int empty = 0;
int wannasleep_sended = 0;

compl_t* completeness;

int end = 0;
pthread_mutex_t mend = PTHREAD_MUTEX_INITIALIZER;
