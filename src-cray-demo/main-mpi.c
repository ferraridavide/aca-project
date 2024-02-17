#include "../c-ray/include/c-ray/c-ray.h"
#include "../c-ray/src/driver/imagefile.h"
#include "../c-ray/src/common/logging.h"
#include "../c-ray/src/common/fileio.h"
#include "../c-ray/src/common/platform/terminal.h"
#include "../c-ray/src/common/timer.h"
#include "../c-ray/src/common/hashtable.h"
#include "../c-ray/src/common/vendored/cJSON.h"
#include "../c-ray/src/common/json_loader.h"
#include "../c-ray/src/common/platform/capabilities.h"
#include "../c-ray/src/driver/encoders/encoder.h"
#include "../c-ray/src/driver/args.h"
#include "../c-ray/src/driver/sdl.h"

#include "../c-ray/src/lib/renderer/renderer.h"
#include "../c-ray/src/lib/datatypes/camera.h"
#include "../c-ray/src/lib/datatypes/scene.h"

#include "./src/my_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mpi.h>
#include <bits/getopt_core.h>

#define MAX_FILENAMES 100
#define MAX_FILENAME_LENGTH 256
#define CHUNK_SIZE 1024

int main(int argc, char *argv[])
{
    bool whole_image = false; // args_is_set(opts, "mode") && strcmp(args_string(opts, "mode"), "sampling");
    int opt;
    while ((opt = getopt(argc, argv, "m:")) != -1)
    {
        switch (opt)
        {
        case 'm':
            if (strcmp(optarg, "sampling") == 0)
            {
                whole_image = true;
            }
        }
    }

    printf("Parallel mode: %s\n", whole_image ? "sampling" : "tiling");

    int rank, size, img_size, img_width, img_height, img_samples, img_tile_height;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Every process needs its own renderer
    struct cr_renderer *renderer = cr_new_renderer();

    const char *jsonFilePath = "./assets/hdr.json";

    bool success = cr_load_json(renderer, jsonFilePath);

    if (success)
    {
        printf("Success!\n");
    }
    else
    {
        printf("Failed!\n");
    }
    printf("Rendering...!\n");

    struct renderer *r = (struct renderer *)renderer;

    img_height = r->prefs.override_height;
    img_width = r->prefs.override_width;
    img_samples = r->prefs.sampleCount;

    img_size = img_height * img_width * 4;

    if (whole_image)
    {
        int samples = img_samples / size;
        if (rank == size - 1)
            samples += img_samples % size;
        printf("Rank %d, samples: %d, img_height: %d \n", rank, samples, img_height);
        my_renderer_render(r, 0, img_height, samples);
    }
    else
    {
        img_tile_height = img_height / size;
        // if (rank == size - 1){
        //     img_tile_height += img_height % size;
        // }
        int tile_index = size - rank - 1;
        printf("Rank %d, samples: %d, img_tile_height: %d \n", rank, img_samples, img_tile_height);
        my_renderer_render(r, tile_index * img_tile_height, (tile_index + 1) * img_tile_height, img_samples);
    }

    char path[20];
    sprintf(path, "%s%d", "output", rank);

    struct cr_bitmap *result = cr_renderer_get_result(renderer);

    float res[img_size];
    MPI_Barrier(MPI_COMM_WORLD);
    printf("All nodes finished rendering...\n");
    if (whole_image)
    {
        MPI_Reduce(result->data.float_ptr, res, img_size, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
    }
    else
    {
        // FIX: Se l'altezza non è divisibile per il numero di nodi , l'immagine esce glitchata
        MPI_Gather(result->data.float_ptr, img_size/size, MPI_FLOAT, res, img_size/size, MPI_FLOAT, 0, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0)
    {
        if (whole_image)
        {
            // Media delle immagini
            for (int i = 0; i < img_size; ++i)
            {
                res[i] /= size;
            }
        }
        printf("Saving...\n");
        struct cr_bitmap bitmap = (struct cr_bitmap){
            .colorspace = cr_bm_linear,
            .precision = cr_bm_float,
            .data = {.float_ptr = res},
            .stride = 4,
            .width = img_width,
            .height = img_height};

        struct imageFile file = (struct imageFile){
            .filePath = "output/",
            .fileName = "output",
            .count = 0,
            .type = png,
            .info = {
                .bounces = 0,
                .samples = 0,
                .crayVersion = cr_get_version(),
                .gitHash = cr_get_git_hash(),
                .threadCount = 0},
            .t = &bitmap};

        writeImage(&file);
    }

    cr_destroy_renderer(renderer);

    MPI_Finalize();

    return 0;
}