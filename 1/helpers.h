#pragma once

#define START_THREAD(x) pthread_t x##_thread; pthread_create(&x##_thread, NULL, x, NULL)
#define WAIT_THREAD(x) pthread_join(x##_thread, NULL)

#define PRINT_COMM(x) do { \
														char name[MPI_MAX_OBJECT_NAME]; \
														int len; \
														MPI_Comm_get_name(x, name, &len); \
														name[len] = 0; \
														printf("%s\n", name); fflush(stdout); \
											} while(0)

#define WRITE_MESSAGE_TABLE do { \
																	printf("--- START OF MESSAGE TABLE ---\nCONNECT -> %d\nGET_PORT -> %d\nWANNA_SLEEP -> %d\nINCOMING -> %d\n--- END OF MESSAGE TABLE ---\n", \
																					CONNECT, GET_PORT, WANNA_SLEEP, INCOMING); \
																} while(0)

#define MINUTE 60
