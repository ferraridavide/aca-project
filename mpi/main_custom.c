#include <stdio.h>
#include <mpi.h>

typedef struct{
    int int_var;
    float float_var;
    char char_var;
}CustomStruct;

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Define the structure
    int block_lengths[3] = {1, 1, 1};
    MPI_Aint displacements[3];
    MPI_Datatype types[3] = {MPI_INT, MPI_FLOAT, MPI_CHAR};

    CustomStruct my_struct;

    // Calculate displacements
    MPI_Get_address(&my_struct.int_var, &displacements[0]);
    MPI_Get_address(&my_struct.float_var, &displacements[1]);
    MPI_Get_address(&my_struct.char_var, &displacements[2]);

    displacements[1] -= displacements[0];
    displacements[2] -= displacements[0];
    displacements[0] = 0;

    MPI_Datatype custom_struct_type;
    MPI_Type_create_struct(3, block_lengths, displacements, types, &custom_struct_type);
    MPI_Type_commit(&custom_struct_type);

    CustomStruct send_data;
    if (rank != 0) {
            send_data.int_var = rank;
            send_data.float_var = rank;
            send_data.char_var = 'A';
            MPI_Send(&send_data, 1, custom_struct_type, 1, 0, MPI_COMM_WORLD);
    }

    CustomStruct recv[size];
    MPI_Gather(&send_data, 1, custom_struct_type, recv, 1, custom_struct_type, 0, MPI_COMM_WORLD);

    if (rank == 0) {
            for (int i=1; i<size; i++) {
                printf("Received data: int=%d, float=%f, char=%c from process %d\n", recv[i].int_var, recv[i].float_var, recv[i].char_var, i);
            }
    }

    MPI_Type_free(&custom_struct_type);
    MPI_Finalize();

    return 0;
}
