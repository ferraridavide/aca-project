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





int main (int argc, char *argv[]) {
    int rank, size, img_size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Every process needs its own renderer
    /* struct cr_renderer *renderer = cr_new_renderer();

    const char *jsonFilePath = "./assets/hdr.json";

    bool success = cr_load_json(renderer, jsonFilePath);

     if (success) {
        printf("Success!\n");
    } else {
        printf("Failed!\n");
    }
    printf("Rendering...!\n");

    struct renderer *r = (struct renderer *)renderer;
    
    r->prefs.tileWidth = r->prefs.override_width; 
    r->prefs.tileHeight = r->prefs.override_height / size;

    img_size = r->prefs.tileWidth * r->prefs.tileHeight * 3;
    */                
    char results[100];
    results[0] = '\0';
    //my_renderer_render(r, size, rank);


        char path [20];
        sprintf(path, "%s%d", "output", rank);
        /* struct imageFile file = (struct imageFile){
                .filePath = "output/",
                .fileName = path,
                .count =  cr_renderer_get_num_pref(renderer, cr_renderer_output_num),
                .type = png,
                .info = {
                    .bounces = cr_renderer_get_num_pref(renderer, cr_renderer_bounces),
                    .samples = cr_renderer_get_num_pref(renderer, cr_renderer_samples),
                    .crayVersion = cr_get_version(),
                    .gitHash = cr_get_git_hash(),
                    .threadCount = cr_renderer_get_num_pref(renderer, cr_renderer_threads)
                },
                .t = cr_renderer_get_result(renderer)
            };
 */
        
        strcat(results, "output/");
        strcat(results, path);
        strcat(results, "_0000.png\n");
        printf("results:%s", results);
        //writeImage(&file);

    char res[1000];
    res[0] = '\0';
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Gather(results, sizeof(results)/sizeof(char), MPI_CHAR, res, sizeof(results)/sizeof(char), MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
        /* for (int i=0; i<1000; i++) {
            printf("%c", res[i]);
        }
        printf("\n"); */

        char file_names[MAX_FILENAMES][MAX_FILENAME_LENGTH];
    

        strcpy(file_names[0], "output/output0_0000.png");
        strcpy(file_names[1], "output/output1_0000.png");
        strcpy(file_names[2], "output/output2_0000.png");
        strcpy(file_names[3], "output/output3_0000.png");
        strcpy(file_names[4], "output/output4_0000.png");

        

        //system("convert -append $(ls -r output/*.png) output/out.png");
        char command[1000] = "./merge.sh";

        // Append each filename as a command-line argument
        for (int i = 0; i < size; i++) {
            sprintf(command + strlen(command), " \"%s\"", file_names[i]);
        }

        // Execute the Bash script
        system("chmod +x merge.sh");
        system(command);
        
        //system("./merge.sh");

        


        printf("Combined PNG files into prova.png successfully.\n");
    }

    //if (rank != 0)
        //cr_destroy_renderer(renderer);

    MPI_Finalize();

    return 0;
}