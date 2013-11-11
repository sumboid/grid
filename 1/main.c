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
#include "compl.h"
#include "state.h"

static void init_state(int argc, char** argv);
static void connect();
static MPI_Comm merge(MPI_Comm comm, int notworker);
static void* listener();
static int port_file_exist();
static void publish_name();
static void unpublish_name();
static void republish_name();
static void read_port();
static void setup();
static void* balancer();
static void give_time(int rank); 
static void sleep_notify(int rank);
static void get_time(int rank);
static void change_sleeptime(int size);
static void selfconnect();

int main(int argc, char *argv[])
{
    init_state(argc, argv);
    setup();
    START_THREAD(listener);
    START_THREAD(balancer);

    compl_set(completeness, get_rank(state), 1);
    while(1) {
        if(sleeptime == 0) {
            if(!wannasleep_sended) { bcast(WANNA_SLEEP); wannasleep_sended = 1; }
            if(compl_check(completeness)) { break; } //Nothing to do

            sleep(1);
            continue;
        }

        wannasleep_sended = 0;
        printf("%d: Zzz\n", get_rank(state)); fflush(stdout);
        change_sleeptime(-1);
        sleep(sleepfragment);
    }

    pthread_mutex_lock(&mend); end = 1; pthread_mutex_unlock(&mend);
    bcast(COMPLETE);
    selfconnect();
    WAIT_THREAD(listener);
    WAIT_THREAD(balancer);
    MPI_Finalize();
    return 0;
}

static void selfconnect() {
    pthread_mutex_lock(&mlistener_stopped);
    if(listener_stopped) {
        pthread_mutex_unlock(&mlistener_stopped);
        return;
    }

    pthread_mutex_unlock(&mlistener_stopped);
    MPI_Comm oldcomm = get_comm(state);
    MPI_Comm newcomm, tmpcomm;
    MPI_Group newgroup;
    MPI_Comm_group(oldcomm, &newgroup);
    MPI_Comm_create(oldcomm, newgroup, &newcomm);
    MPI_Comm_connect(mpi_port, MPI_INFO_NULL, 0, newcomm, &tmpcomm);
}

static void init_state(int argc, char** argv) {
    int required = MPI_THREAD_MULTIPLE;
    int provided = 0;
    MPI_Init_thread(&argc, &argv, required, &provided);

    state = create_state();
    completeness = compl_init(state);
}

static void setup() {
    if(get_rank(state) == 0) {
        if(!port_file_exist()) {
            sleeptime = 5 * MINUTE;
            publish_name(); 
            bcast(GET_PORT);
            MPI_Barrier(get_comm(state));
        }
        else {
            bcast(CONNECT);
            MPI_Barrier(get_comm(state));
            connect();
        }
    } else {
        message m = recv_message().m;
        MPI_Barrier(get_comm(state));
        if(m == CONNECT)  connect();
        else if(m == GET_PORT) read_port();
    }
}



static void connect() {
    MPI_Comm newcomm;
    read_port();
    MPI_Comm_connect(mpi_port, MPI_INFO_NULL, 0, get_comm(state), &newcomm);
    newcomm = merge(newcomm, 1);
    update_state(state, newcomm);
    compl_resize(completeness, state);
}

static MPI_Comm merge(MPI_Comm comm, int notworker) {
    MPI_Comm newcomm;
    MPI_Intercomm_merge(comm, notworker, &newcomm);
    return newcomm;
}

static void* listener() {
    while(1) {
        MPI_Comm newcomm;
        MPI_Comm tmpcomm;

        int err = MPI_Comm_accept(mpi_port, MPI_INFO_NULL, 0, get_comm(state), &tmpcomm);
        if(err != MPI_SUCCESS) printf("Error: %d\n", err);

        pthread_mutex_lock(&mend);
        if(end) {
            pthread_mutex_unlock(&mend);
            pthread_mutex_lock(&mlistener_stopped);
            listener_stopped = 1;
            pthread_mutex_unlock(&mlistener_stopped);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&mend);

        newcomm = merge(tmpcomm, 0);
        if(get_rank(state) == 0) republish_name();
        update_state(state, newcomm);
        compl_resize(completeness, state);
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

static void* balancer() {
    while(1) {
        message_t mes = recv_message();
        if(mes.rank == -1) pthread_exit(NULL);
        switch(mes.m) {
            case WANNA_SLEEP: give_time(mes.rank); break;
            case INCOMING: get_time(mes.rank); break;
            case SLEEP: sleep_notify(mes.rank); break;
            case COMPLETE: compl_set(completeness, mes.rank, 1); break;
            case END: pthread_exit(NULL); break;
            default: printf("%d: not cool: %d\n", get_rank(state), mes.m); fflush(stdout); break;
        }
    }
}

static void give_time(int rank) {
    int fragments = (sleeptime / sleepfragment) / (get_size(state) - get_oldsize(state) + 1);
    if(fragments == 0) {
        compl_set(completeness, rank, 1);
        return;
    }

    change_sleeptime(-fragments);
    compl_set(completeness, rank, 0);
    send_message(rank, INCOMING);
    send_fragments(rank, fragments);
}

static void sleep_notify(int rank) {
    compl_set(completeness, rank, 0);
}

static void get_time(int rank) {
    bcast(SLEEP);
    int fragments = recv_fragments(rank);
    change_sleeptime(fragments);
}

static void change_sleeptime(int size) {
    pthread_mutex_lock(&sleep_mutex);
    sleeptime += size * sleepfragment;
    pthread_mutex_unlock(&sleep_mutex);
}

