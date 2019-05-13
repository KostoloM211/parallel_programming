#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "mpi.h"
#include <string.h>

const int MX = 640;
const int MY = 480;

const int NUM_ITER = 100;

const int MESSAGE_TAG = 123;
const int WRITE_TAG = 65;

float** input;
float** newInput;
void* tmp;

int rank = 0;
int size = 0;
int rowsCount = 0;

void printRes(float** res) {
    int i, j;
    for (i = 0; i < rowsCount; i++) {
        for (j = 0; j < MY; j++) {
            printf("%f ", res[i][j]);
        }
        printf("\n");
    }
}

void readLocalData() {
    // printf("rank:%d\n", rank);
    // fflush(stdout)
    int m1 = (MX - 2) / size + 2;

    // printf("m1 = %d\n", m1);

    int rem = (MX - 2) % size; 

    // printf("rem = %d\n", rem);

    int m2 = rem > 0 ? m1 + 1 : m1;

    // printf("m2 = %d\n", m2);

    int startRow = 0;
    if (rank < rem) {
        startRow = rank * (m2 - 2);
        rowsCount = m2;
    }
    else {
        startRow = rem * (m2 - 2) + (rank - rem) * (m1 - 2);
        rowsCount = m1;
    }

    FILE *fp = fopen("/home/kosta3ov/lab1/input", "rb");   
    fseek(fp, sizeof(float) * startRow * MY, SEEK_SET);

    // printf("startRow = %d\n", startRow);
    // printf("rowCount = %d\n", rowsCount);
    // fflush(stdout);


    input = (float**)malloc(sizeof(float*) * rowsCount);
    newInput = (float**)malloc(sizeof(float*) * rowsCount);

    int i, j;
    for (i = 0; i < rowsCount; i++) {
        input[i] = (float*)malloc(sizeof(float) * MY);
        newInput[i] = (float*)malloc(sizeof(float) * MY);

        int readed = fread(input[i], sizeof(float), MY, fp);
        if (readed != MY) {
            // printf("error reading data:\n");
            // fflush(stdout);

        }
        for (j = 0; j < MY; j++) {
            newInput[i][j] = input[i][j];
        }
    }
    return;
}

void processData(int startRow, int endRow) {
    int i, j;
    for (i = startRow; i < endRow; i++) {
        for (j = 1; j < MY - 1; j++) {
            newInput[i][j] = (input[i][j+1] + input[i][j-1] + input[i-1][j] + input[i+1][j]) * 0.25;
        }
    }    
}

void sendToUp(int dest, MPI_Request* send) {
    int calculatedRow = 1;
    MPI_Isend(newInput[calculatedRow], MY, MPI_FLOAT, dest, MESSAGE_TAG, MPI_COMM_WORLD, send);
}

void receiveFromUp(int dest, MPI_Request* recieve) {
    int receiveRow = 0;
    MPI_Irecv(newInput[receiveRow], MY, MPI_FLOAT, dest, MESSAGE_TAG, MPI_COMM_WORLD, recieve);
}

void recieveFromDown(int dest, MPI_Request* recieve) {
    int requestRow = rowsCount - 1;
    MPI_Irecv(newInput[requestRow], MY, MPI_FLOAT, dest, MESSAGE_TAG, MPI_COMM_WORLD, recieve);
}

void sendToDown(int dest, MPI_Request* send) {
    int sendRow = rowsCount - 2;
    MPI_Isend(newInput[sendRow], MY, MPI_FLOAT, dest, MESSAGE_TAG, MPI_COMM_WORLD, send);
}

void createOutputFile() {
    FILE* fo = fopen("/home/kosta3ov/lab1/output", "wb");
    fclose(fo);
}

void writeToOutputFile() {
    FILE* fo = fopen("/home/kosta3ov/lab1/output", "a+b");
    int i;

    if (rank == 0) {
        fwrite(input[0], sizeof(float), MY, fo);
    }

    for (i = 1; i < rowsCount - 1; i++) {
        fwrite(input[i], sizeof(float), MY, fo);
    }

    if (rank == size - 1) {
        fwrite(input[rowsCount - 1], sizeof(float), MY, fo);
    }

    fclose(fo);
}

void solve() {
    int countRequests = rank > 0 && rank < size - 1 ? 2 : 1;
    //Перед отправкой
    if (rank != 0) {
        int calculatedRow = 1;
        processData(calculatedRow, calculatedRow + 1);
    }

    //Создание запросов
    MPI_Request* requests = (MPI_Request*)malloc(sizeof(MPI_Request) * countRequests);
    MPI_Status* statuses = (MPI_Status*)malloc(sizeof(MPI_Status) * countRequests);

    if (rank == 0) {
        createOutputFile();
        recieveFromDown(1, &requests[0]);
        processData(1, rowsCount - 2);
    }
    else if (rank == size - 1) {
        sendToUp(size - 2, &requests[0]);
        processData(2, rowsCount - 1);
    }
    else {
        sendToUp(rank - 1, &requests[0]);
        recieveFromDown(rank + 1, &requests[1]);
        processData(2, rowsCount - 2);
    }
    
    MPI_Waitall(countRequests, requests, statuses);

    requests = (MPI_Request*)malloc(sizeof(MPI_Request) * countRequests);

    ///После отправки
    if (rank != size - 1) {
        int lastCalculatedRow = rowsCount - 2;
        processData(lastCalculatedRow, lastCalculatedRow + 1);
    }

    if (rank == 0) {
        sendToDown(1, &requests[0]);
    }
    else if (rank == size - 1) {
        receiveFromUp(size -2, &requests[0]);
    }
    else {
        receiveFromUp(rank - 1, &requests[0]);
        sendToDown(rank + 1, &requests[1]);
    }

    MPI_Waitall(countRequests, requests, statuses);

    tmp = input;
    input = newInput;
    newInput = tmp;
}

void writeResult() {
    MPI_Status st;
    if (rank == 0) {
        writeToOutputFile();
        MPI_Send(NULL, 0, MPI_DOUBLE, 1, WRITE_TAG, MPI_COMM_WORLD);
    }
    else if (rank == size - 1) {    
        MPI_Recv(NULL, 0, MPI_DOUBLE, size - 2, WRITE_TAG, MPI_COMM_WORLD, &st);
        writeToOutputFile();
    }
    else {
        MPI_Recv(NULL, 0, MPI_DOUBLE, rank - 1, WRITE_TAG, MPI_COMM_WORLD, &st);
        writeToOutputFile();
        MPI_Send(NULL, 0, MPI_DOUBLE, rank + 1, WRITE_TAG, MPI_COMM_WORLD);
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    readLocalData();

    double t1 = MPI_Wtime();
    
    if (rank == 0 && size == 1) {
        int i;
        for (i = 0; i < NUM_ITER; i++) {
            processData(1, MX - 1);
        }
    }
    else {
        int i;
        for (i = 0; i < NUM_ITER; i++) {
            solve();
        }
    }
    
    double t2 = MPI_Wtime();

    double solveTime = t2 - t1;
    printf("time = %lf\n", solveTime);
    fflush(stdout);

    writeResult();

    MPI_Finalize();
    return 0;
}
