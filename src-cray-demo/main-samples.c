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

#include "./src/my_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MAX_FILENAMES 100
#define MAX_FILENAME_LENGTH 256
#define CHUNK_SIZE 1024

int main(int argc, char *argv[])
{
    int rank, size, img_size;

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
    r->prefs.sampleCount = 50;

    cr_renderer_render((struct cr_renderer *)r);

    char path[20];
    sprintf(path, "%s%d", "output", rank);

    struct cr_bitmap *result = cr_renderer_get_result(renderer);

    img_size = r->prefs.override_height * r->prefs.override_height * 4;
    float res[img_size];
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Reduce(result->data.float_ptr, res, img_size, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0)
    {

        for (int i=0; i<img_size; i++) {
            res[i] /= size;
        }

        struct cr_bitmap bitmap = (struct cr_bitmap){
            .colorspace = cr_bm_linear,
            .precision = cr_bm_float,
            .data = {.float_ptr = res},
            .stride = 4,
            .width = 250,
            .height = 250};

        struct imageFile file = (struct imageFile){
            .filePath = "output/",
            .fileName = "output",
            .count = cr_renderer_get_num_pref(renderer, cr_renderer_output_num),
            .type = png,
            .info = {
                .bounces = cr_renderer_get_num_pref(renderer, cr_renderer_bounces),
                .samples = cr_renderer_get_num_pref(renderer, cr_renderer_samples),
                .crayVersion = cr_get_version(),
                .gitHash = cr_get_git_hash(),
                .threadCount = 0},
            .t = &bitmap
            };

        writeImage(&file);
    }

    cr_destroy_renderer(renderer);

    MPI_Finalize();

    return 0;
}