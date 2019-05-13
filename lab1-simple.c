#include <stdio.h>
#include <time.h>
#include "mpi.h"

#define MX 640
#define MY 480

#define NITER 7208
#define STEPITER 100

static float f0[MX][MY];
static float f1[MX][MY];

int main( int argc, char **argv ) {
    MPI_Init(&argc, &argv);

    int i, j, n, m;
    FILE *fp;

    float (*fold)[MY];
    float (*fnew)[MY];

    void *ptmp;

    printf("Solving heat conduction task on %d by %d grid\n", MX, MY );
    
    fflush(stdout);
    fold = f0;
    fnew = f1;

    /* Initial conditions: */
    for (i = 0; i < MX; i++) {
        for (j = 0; j < MY; j++) {
            fold[i][j] = fnew[i][j] = 0.0;
            if ((i == 0) || (j == 0)) {
                fold[i][j] = fnew[i][j] = 1.0;
            }
            else if ((i == (MX-1)) || (j == (MY-1))) { 
                fold[i][j] = fnew[i][j] = 0.5;
            }
        }   
    }
    
    double t1 = MPI_Wtime();

    /* Iteration loop: */
    for (n = 0; n < NITER; n++) {
        /* Step of calculation starts here: */
        for (i = 1; i < (MX-1); i++) {
            for (j = 1; j < (MY-1); j++) {
                fnew[i][j] = ( fold[i][j+1] + fold[i][j-1] + fold[i-1][j] + fold[i+1][j] ) * 0.25;
            }
        }
        /* Exchange old/new pointers: */
        ptmp = fold;
        fold = fnew;
        fnew = ptmp;
    }
    
    double solveTime = MPI_Wtime() - t1;
    printf("time = %lf\n", solveTime);

    /* Calculation is done, fold array is a result: */

    fp = fopen("output-simple", "wb");
    for (i = 0; i < (MX); i++) {
        fwrite(fold[i], sizeof(float), MY, fp);
    }
    fclose(fp);

    MPI_Finalize();
    return 0;
}
