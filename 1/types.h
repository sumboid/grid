#pragma once
#include <pthread.h>

typedef enum {
    CONNECT,
    GET_PORT,
    WANNA_SLEEP,
    SLEEP,
    COMPLETE,
    INCOMING,
    END
} message;

enum {
    MESSAGE,
    JOB
};

typedef struct {
    int rank;
    message m;
} message_t;


