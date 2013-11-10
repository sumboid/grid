#pragma once

#include <mpi.h>
#include "types.h"
#include "global.h"

void bcast(message m);
message_t recv_message();
void send_message(int rank, message m);
void send_fragments(int rank, int fragments);
int recv_fragments(int rank);
