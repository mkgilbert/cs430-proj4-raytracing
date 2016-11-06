/** raycast main program entry point
 *  Author: Michael Gilbert
 *
 *  reads in a json file and raycasts to image */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../include/json.h"
#include "../include/vector_math.h"
#include "../include/raytracer.h"
#include "../include/ppmrw.h"
#include "../include/base.h"

/* example usage: raytrace width height input.json out.ppm */
int main(int argc, char *argv[]) {
    /* testing that we can read json objects */
    if (argc != 5) {
        fprintf(stderr, "Error: main: You must have 4 arguments\n");
        exit(1);
    }
    /* test dimensions */
    if (atoi(argv[1]) <= 0 || atoi(argv[2]) <= 0) {
        fprintf(stderr, "Error: main: width and height parameters must be > 0\n");
        exit(1);
    }

    /* open the input json file */
    FILE *json = fopen(argv[3], "rb");
    if (json == NULL) {
        fprintf(stderr, "Error: main: Failed to open input file '%s'\n", argv[3]);
        exit(1);
    }

    /* initialize object and light arrays to have all null values */
    init_lights();
    init_objects();

    /* fill object and light arrays with scene info */
    read_json(json);

    /* create image */
    image img;
    img.width = atoi(argv[1]);
    img.height = atoi(argv[2]);
    img.pixmap = (RGBPixel*) malloc(sizeof(RGBPixel)*img.width*img.height);
    //print_pixels(img.pixmap, img.width, img.height);
    int pos = get_camera(objects);
    if (pos == -1) {
        fprintf(stderr, "Error: main: No camera object found in data\n");
        exit(1);
    }

    /* fill the img->pixmap with colors by raycasting the objects */
    raycast_scene(&img, objects[pos].camera.width, objects[pos].camera.height, objects);

    /* create output file and write image data */
    FILE *out = fopen(argv[4], "wb");
    if (out == NULL) {
        fprintf(stderr, "Error: main: Failed to create output file '%s'\n", argv[4]);
        exit(1);
    }
    create_ppm(out, 6, &img);
    /* cleanup */
    fclose(out);

    return 0;
}