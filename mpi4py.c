#include "mpi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NP 1000

const int NUM_ITER = 10000;
const int MESSAGE_TAG = 123;
const int WRITE_TAG = 65;

static int nfp = 0;
static char *ps[NP];
static char **p = ps;

static MPI_Request* requests;
static MPI_Status* statuses;
static int countRequests = 0;

static int mx = 0;
static int my = 0;

void sendDimensions(int xdim, int ydim) {
    mx = xdim;
    my = ydim;
}

void allocRequests(int count) {
    requests = (MPI_Request*)malloc(sizeof(MPI_Request) * count);
    statuses = (MPI_Status*)malloc(sizeof(MPI_Status) * count);
    countRequests = count;
}

void waitRequests() {
    MPI_Waitall(countRequests, requests, statuses);
}

double tick() {
    return MPI_Wtime();
}

void finalize() {
    MPI_Finalize();
}

void sendSynchromessage(int to) {
    MPI_Send(NULL, 0, MPI_DOUBLE, to, WRITE_TAG, MPI_COMM_WORLD);
}

void recvSynchromessage(int from) {
    MPI_Status st;
    MPI_Recv(NULL, 0, MPI_DOUBLE, from, WRITE_TAG, MPI_COMM_WORLD, &st);
}

void sendToUp(float* from, int dest) {
    int calculatedRow = 1;
    MPI_Isend(from + calculatedRow * my, my, MPI_FLOAT, dest, MESSAGE_TAG, MPI_COMM_WORLD, requests);
}

void recieveFromDown(float* from, int dest) {
    int requestRow = mx - 1;
    int send = 0;
    if (countRequests == 2) {
        send = 1;
    }
    MPI_Irecv(from + requestRow * my, my, MPI_FLOAT, dest, MESSAGE_TAG, MPI_COMM_WORLD, &requests[send]);
}

void receiveFromUp(float* from, int dest) {
    int receiveRow = 0;
    MPI_Irecv(from + receiveRow * my, my, MPI_FLOAT, dest, MESSAGE_TAG, MPI_COMM_WORLD, requests);
}

void sendToDown(float* from, int dest) {
    int sendRow = mx - 2;
    int send = 0;
    if (countRequests == 2) {
        send = 1;
    }
    MPI_Isend(from + sendRow * my, my, MPI_FLOAT, dest, MESSAGE_TAG, MPI_COMM_WORLD, &requests[send]);
}

int mpiinit() {
    MPI_Init(&nfp, &p);
    return 0;
}
    
int ranksize(int* rank, int* size) {
    MPI_Comm_rank(MPI_COMM_WORLD, rank);
    MPI_Comm_size(MPI_COMM_WORLD, size);
    return 0;
}
