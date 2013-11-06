#pragma once

#include <mpi.h>
#include "types.h"
#include "global.h"

void mpi_send(int message, int tag, int to);
message_t mpi_recv(int tag, int from);
