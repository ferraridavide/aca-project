#include "my_render.h"
#include "../../c-ray/src/lib/datatypes/camera.h"
#include "../../c-ray/src/lib/datatypes/scene.h"
#include "../../c-ray/src/lib/renderer/pathtrace.h"
#include "../../c-ray/src/common/platform/signal.h"
#include "../../c-ray/src/lib/accelerators/bvh.h"
#include <stdio.h>

//Main thread loop speeds
#define paused_msec 100
#define active_msec  16

struct tile_set my_tile_quantize(unsigned width, unsigned height, unsigned tile_w, unsigned tile_h, unsigned rank) {

	logr(info, "Quantizing render plane\n");

	struct tile_set set = { 0 };
	set.tile_mutex = mutex_create();

	//Sanity check on tilesizes
	if (tile_w >= width) tile_w = width;
	if (tile_h >= height) tile_h = height;
	if (tile_w <= 0) tile_w = 1;
	if (tile_h <= 0) tile_h = 1;

	unsigned tiles_x = width / tile_w;
	unsigned tiles_y = height / tile_h;

	tiles_x = (width % tile_w) != 0 ? tiles_x + 1: tiles_x;
	tiles_y = (height % tile_h) != 0 ? tiles_y + 1: tiles_y;

	int tileCount = 0;
	for (unsigned y = 0; y < tiles_y; ++y) {
		for (unsigned x = 0; x < tiles_x; ++x) {
			struct render_tile tile = { 0 };
			tile.width  = tile_w;
			tile.height = tile_h;
			
			tile.begin.x = x       * tile_w;
			tile.end.x   = (x + 1) * tile_w;
			
			tile.begin.y = y       * tile_h;
			tile.end.y   = (y + 1) * tile_h;
			
			tile.end.x = min((x + 1) * tile_w, width);
			tile.end.y = min((y + 1) * tile_h, height);
			
			tile.width = tile.end.x - tile.begin.x;
			tile.height = tile.end.y - tile.begin.y;

			tile.state = ready_to_render;

			tile.index = tileCount++;

            if (tile.index == rank){
    			render_tile_arr_add(&set.tiles, tile);
            }
		}
	}
	logr(info, "Quantized image into %i tiles. (%ix%i)\n", (tiles_x * tiles_y), tiles_x, tiles_y);

	return set;
}



void *my_render_thread(void *arg) {
    block_signals();
	struct worker *threadState = (struct worker*)thread_user_data(arg);
	// Aggiungere int seed al "worker" in "renderer.h"
	int seed = threadState->seed;
	struct renderer *r = threadState->renderer;
	struct texture *buf = threadState->buf;
	sampler *sampler = newSampler();

	struct camera *cam = threadState->cam;

	//First time setup for each thread
	struct render_tile *tile = tile_next(threadState->tiles);
	threadState->currentTile = tile;
	
	struct timeval timer = { 0 };
	size_t samples = 1;
	
	while (tile && r->state.rendering) {
		long total_us = 0;
		
		while (samples < r->prefs.sampleCount + 1 && r->state.rendering) {
			timer_start(&timer);
			for (int y = tile->height - 1; y >= 0; --y) {
				for (int x = tile->begin.x; x < tile->end.x; ++x) {
					if (r->state.render_aborted) goto exit;
					uint32_t pixIdx = (uint32_t)(y * buf->width + x);
					initSampler(sampler, Random, samples - 1, r->prefs.sampleCount, pixIdx + seed);
					
					struct color output = textureGetPixel(buf, x, y, false);
					struct color sample = path_trace(cam_get_ray(cam, x, y + tile->begin.y, sampler), r->scene, r->prefs.bounces, sampler);
					
					// Clamp out fireflies - This is probably not a good way to do that.
					nan_clamp(&sample, &output);

					//And process the running average
					output = colorCoef((float)(samples - 1), output);
					output = colorAdd(output, sample);
					float t = 1.0f / samples;
					output = colorCoef(t, output);
					
					//Store internal render buffer (float precision)
					setPixel(buf, output, x, y);
				}
			}
			//For performance metrics
			total_us += timer_get_us(timer);
			threadState->totalSamples++;
			samples++;
			tile->completed_samples++;
			//Pause rendering when bool is set
			while (threadState->paused && !r->state.render_aborted) {
				timer_sleep_ms(100);
			}
			threadState->avg_per_sample_us = total_us / samples;
		}
		//Tile has finished rendering, get a new one and start rendering it.
		tile->state = finished;
		threadState->currentTile = NULL;
		samples = 1;
		tile = tile_next(threadState->tiles);
		threadState->currentTile = tile;
	}
exit:
	destroySampler(sampler);
	//No more tiles to render, exit thread. (render done)
	threadState->thread_complete = true;
	threadState->currentTile = NULL;
	return 0;
}

static void my_print_stats(const struct world *scene) {
	uint64_t polys = 0;
	uint64_t vertices = 0;
	uint64_t normals = 0;
	for (size_t i = 0; i < scene->instances.count; ++i) {
		if (isMesh(&scene->instances.items[i])) {
			const struct mesh *mesh = &scene->meshes.items[scene->instances.items[i].object_idx];
			polys += mesh->polygons.count;
			vertices += mesh->vbuf->vertices.count;
			normals += mesh->vbuf->normals.count;
		}
	}
	logr(info, "Totals: %liV, %liN, %zuI, %liP, %zuS, %zuM\n",
		   vertices,
		   normals,
		   scene->instances.count,
		   polys,
		   scene->spheres.count,
		   scene->meshes.count);
}

void my_renderer_render(struct renderer *r, unsigned from, unsigned to, unsigned samples, int seed) {
    struct camera camera = r->scene->cameras.items[r->prefs.selected_camera];
	if (r->prefs.override_width && r->prefs.override_height) {
		camera.width = r->prefs.override_width ? (int)r->prefs.override_width : camera.width;
		camera.height = r->prefs.override_height ? (int)r->prefs.override_height : camera.height;
		cam_recompute_optics(&camera);
	}

	// Verify we have at least a single thread rendering.
	// if (r->state.clients.count == 0 && r->prefs.threads < 1) {
	// 	logr(warning, "No network render workers, setting thread count to 1\n");
	// 	r->prefs.threads = 1;
	// }

	if (!r->scene->background) {
		r->scene->background = newBackground(&r->scene->storage, NULL, NULL, NULL, r->scene->use_blender_coordinates);
	}
	
	//struct tile_set set = my_tile_quantize(camera.width, camera.height, r->prefs.tileWidth, r->prefs.tileHeight, rank);

	r->prefs.sampleCount = samples;
	struct tile_set set = { 0 };
	set.tile_mutex = mutex_create();

	struct render_tile tile = { 0 };
	tile.begin.x = 0;
	tile.end.x   = camera.width;

	tile.begin.y = from;
	tile.end.y   = to;

	tile.width = tile.end.x - tile.begin.x;
	tile.height = tile.end.y - tile.begin.y;

	tile.state = ready_to_render;

	tile.index = 0;

	render_tile_arr_add(&set.tiles, tile);

	for (size_t i = 0; i < r->scene->shader_buffers.count; ++i) {
		if (!r->scene->shader_buffers.items[i].bsdfs.count) {
			logr(warning, "bsdf buffer %zu is empty, patching in placeholder\n", i);
			bsdf_node_ptr_arr_add(&r->scene->shader_buffers.items[i].bsdfs, build_bsdf_node((struct cr_scene *)r->scene, NULL));
		}
	}
	// Bind object buffers to instances
	for (size_t i = 0; i < r->scene->instances.count; ++i) {
		struct instance *inst = &r->scene->instances.items[i];
		inst->bbuf = &r->scene->shader_buffers.items[inst->bbuf_idx];
	}
	
	for (size_t i = 0; i < r->scene->meshes.count; ++i) {
		struct mesh *m = &r->scene->meshes.items[i];
		m->vbuf = &r->scene->v_buffers.items[m->vbuf_idx];
	}

	// Do some pre-render preparations
	// Compute BVH acceleration structures for all meshes in the scene
	compute_accels(r->scene->meshes);

	// And then compute a single top-level BVH that contains all the objects
	if (r->scene->instances_dirty) {
		logr(info, "%s top-level BVH: ", r->scene->topLevel ? "Updating" : "Computing");
		if (r->scene->topLevel) destroy_bvh(r->scene->topLevel);
		struct timeval bvh_timer = {0};
		timer_start(&bvh_timer);
		r->scene->topLevel = build_top_level_bvh(r->scene->instances);
		printSmartTime(timer_get_ms(bvh_timer));
		logr(plain, "\n");
		r->scene->instances_dirty = false;
	}

	my_print_stats(r->scene);

	for (size_t i = 0; i < set.tiles.count; ++i)
		set.tiles.items[i].total_samples = r->prefs.sampleCount;

	//Print a useful warning to user if the defined tile size results in less renderThreads
	if (set.tiles.count < r->prefs.threads) {
		logr(warning, "WARNING: Rendering with a less than optimal thread count due to large tile size!\n");
		logr(warning, "Reducing thread count from %zu to %zu\n", r->prefs.threads, set.tiles.count);
		r->prefs.threads = set.tiles.count;
	}

    // Render buffer is used to store accurate color values for the renderers' internal use
	// if (!r->state.result_buf) {
		// Allocate
		r->state.result_buf = newTexture(float_p, tile.width, tile.height, 4);
	// } else if (r->state.result_buf->width != (size_t)camera.width || r->state.result_buf->height != (size_t)camera.height) {
	// 	// Resize
	// 	if (r->state.result_buf) destroyTexture(r->state.result_buf);
	// 	r->state.result_buf = newTexture(float_p, camera.width, camera.height, 4);
	// } else {
	// 	// Clear
	// 	tex_clear(r->state.result_buf);
	// }

    struct texture *result = r->state.result_buf;

	struct cr_tile *info_tiles = calloc(set.tiles.count, sizeof(*info_tiles));
	struct cr_renderer_cb_info cb_info = {
		.tiles = info_tiles,
		.tiles_count = set.tiles.count,
		.fb = (struct cr_bitmap *)result,
	};
	
	struct callback start = r->state.callbacks[cr_cb_on_start];
	if (start.fn) {
		//update_cb_info(r, &set, &cb_info);
		start.fn(&cb_info, start.user_data);
	}

	logr(info, "Pathtracing%s...\n", r->prefs.iterative ? " iteratively" : "");

    r->state.rendering = true;
	r->state.render_aborted = false;

    if (r->state.clients.count) logr(info, "Using %lu render worker%s totaling %lu thread%s.\n", r->state.clients.count, PLURAL(r->state.clients.count), r->state.clients.count, PLURAL(r->state.clients.count));
	
	// Select the appropriate renderer type for local use
	void *(*local_render_thread)(void *) = my_render_thread;
	// Iterative mode is incompatible with network rendering at the moment
	// if (r->prefs.iterative && !r->state.clients.count) local_render_thread = render_thread_interactive;
	
	// Create & boot workers (Nonblocking)
	// Local render threads + one thread for every client
	for (size_t t = 0; t < r->prefs.threads; ++t) {
		worker_arr_add(&r->state.workers, (struct worker){
			.renderer = r,
			.seed = seed,
			.buf = result,
			.cam = &camera,
			.thread = (struct cr_thread){
				.thread_fn = local_render_thread,
			}
		});
	}
	// for (size_t c = 0; c < r->state.clients.count; ++c) {
	// 	worker_arr_add(&r->state.workers, (struct worker){
	// 		.client = &r->state.clients.items[c],
	// 		.renderer = r,
	// 		.buf = result,
	// 		.cam = &camera,
	// 		.thread = (struct cr_thread){
	// 			.thread_fn = client_connection_thread
	// 		}
	// 	});
	// }
	for (size_t w = 0; w < r->state.workers.count; ++w) {
		r->state.workers.items[w].thread.user_data = &r->state.workers.items[w];
		r->state.workers.items[w].tiles = &set;
		if (thread_start(&r->state.workers.items[w].thread))
			logr(error, "Failed to start worker %zu\n", w);
	}

	//Start main thread loop to handle renderer feedback and state management
	while (r->state.rendering) {
		
		struct callback status = r->state.callbacks[cr_cb_status_update];
		if (status.fn) {
			//update_cb_info(r, &set, &cb_info);
			status.fn(&cb_info, status.user_data);
		}

		size_t inactive = 0;
		for (size_t w = 0; w < r->state.workers.count; ++w) {
			if (r->state.workers.items[w].thread_complete) inactive++;
		}
		if (r->state.render_aborted || inactive == r->state.workers.count)
			r->state.rendering = false;
		timer_sleep_ms(r->state.workers.items[0].paused ? paused_msec : active_msec);
	}
	
	//Make sure render threads are terminated before continuing (This blocks)
	for (size_t w = 0; w < r->state.workers.count; ++w) {
		thread_wait(&r->state.workers.items[w].thread);
	}
	struct callback stop = r->state.callbacks[cr_cb_on_stop];
	if (stop.fn) {
		//update_cb_info(r, &set, &cb_info);
		stop.fn(&cb_info, stop.user_data);
	}
	if (info_tiles) free(info_tiles);
	tile_set_free(&set);
}