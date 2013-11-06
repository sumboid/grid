#pragma once
#include <mpi.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "types.h"

extern mpi_state_t* state;

extern char mpi_port[MPI_MAX_PORT_NAME];
extern FILE* port_file;

extern pthread_mutex_t sleep_mutex;
extern int sleeptime;
extern int sleepfragment;
extern int empty;
extern int wannasleep_sended;