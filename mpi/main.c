#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

void rand_matrix(int mat[3][3]) {
    srand(time(NULL));

    for (int i=0; i<3; i++) {
        for (int j=0; j<3;j++) {
            mat[i][j] = rand() % 10;
        }
    }
}

int main (int argc, char *argv[]) {
    int rank, size;
    char path[] = "text.txt";
    char content[100];
    int num, mat[3][3];
    FILE *fp;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        MPI_Bcast(&path, 1, MPI_CHAR, 0, MPI_COMM_WORLD);     
    }

    if (rank != 0) {
        fp = fopen(path, "r");
        fgets(content, 100, fp);
        fclose(fp);
        printf("%s\n", content);
        rand_matrix(mat);
    }

    int res[63];
    MPI_Gather(mat, 9, MPI_INT, res, 9, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Length of received vector: %lu\n", sizeof(res) / sizeof(res[0]));
        for (int i=0; i<63; i++) {
        printf("%d-", res[i]);
        }
        printf("\n");
    }

    MPI_Finalize();

    return 0;
}