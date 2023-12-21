#include "../c-ray/c-ray/include/c-ray/c-ray.h"
#include "../c-ray/c-ray/src/driver/imagefile.h"
#include "../c-ray/c-ray/src/driver/encoders/encoder.h"
#include <stdio.h>

int main() {
     // Initialize the c-ray renderer
    // int *renderer = cr_renderer_();
    struct cr_renderer *renderer = cr_new_renderer();

    const char *jsonFilePath = "../c-ray/c-ray/input/hdr.json";

    // Load the JSON file into the cr_renderer
    bool success = cr_load_json(renderer, jsonFilePath);

     if (success) {
        printf("Success!\n");
    } else {
        printf("Failed!\n");
    }
    printf("Rendering...!\n");

    struct cr_bitmap *final = cr_renderer_render(renderer);
    

    struct imageFile file = (struct imageFile){
			.filePath = "output/",
			.fileName = "image.png",
			.count =  cr_renderer_get_num_pref(renderer, cr_renderer_output_num),
			.type = png,
			.info = {
				.bounces = cr_renderer_get_num_pref(renderer, cr_renderer_bounces),
				.samples = cr_renderer_get_num_pref(renderer, cr_renderer_samples),
				.crayVersion = cr_get_version(),
				.gitHash = cr_get_git_hash(),
				.threadCount = cr_renderer_get_num_pref(renderer, cr_renderer_threads)
			},
			.t = final
		};
    writeImage(&file);
    printf("Rendered!\n");
    cr_bitmap_free(final);
    cr_destroy_renderer(renderer);

    return 0;
    // printf("Hello, World!\n");
    // return 0;
}
