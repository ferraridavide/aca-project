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



int main() {
     // Initialize the c-ray renderer
    // int *renderer = cr_renderer_();
    struct cr_renderer *renderer = cr_new_renderer();

    const char *jsonFilePath = "../aca-project/src-cray-demo/assets/hdr.json";

    // Load the JSON file into the cr_renderer
    bool success = cr_load_json(renderer, jsonFilePath);

     if (success) {
        printf("Success!\n");
    } else {
        printf("Failed!\n");
    }
    printf("Rendering...!\n");

    // cr_renderer_render(renderer);
    struct renderer *r = (struct renderer *)renderer;
    // renderer_render(r);



    unsigned sizeOfWorkers = 4;
    unsigned rank = 3; // [0, sizeOfWorkers - 1]
    printf("Rank is: %d\n", rank);
    
    r->prefs.sampleCount = 250;
    r->prefs.override_height = 500;
    r->prefs.override_width = 500;
    r->prefs.tileWidth = r->prefs.override_width; 
    r->prefs.tileHeight = r->prefs.override_height / sizeOfWorkers;

    my_renderer_render(r, sizeOfWorkers, rank);


    struct imageFile file = (struct imageFile){
			.filePath = "output/",
			.fileName = "output.png",
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
    writeImage(&file);
    printf("Rendered!\n");
    cr_destroy_renderer(renderer);

    


    //(x + (t->height - (y + 1)) * t->width) * t->channels + 0
    for (int i = 0; i < 100; ++i) {
        printf("array[%d] = %.2f\n", i, file.t->data.float_ptr[i]);
    }

    return 0;
    // printf("Hello, World!\n");
    // return 0;
}