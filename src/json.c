//
// Created by mkg on 10/7/2016.
//
/* json.c parses json files for view objects */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "../include/json.h"

/* global variables */
int line = 1;                   // global var for line numbers as we parse
object objects[MAX_OBJECTS];    // allocate space for all objects in json file
Light lights[MAX_OBJECTS];      // allocate space for lights
int nlights;
int nobjects;

/* helper functions */

// next_c wraps the getc function that provides error checking and line #
// Problem: if we do ungetc, it could screw us up on the line #
int next_c(FILE* json) {
    int c = fgetc(json);
#ifdef DEBUG
    printf("next_c: '%c'\n", c);
#endif
    if (c == '\n') {
        line++;;
    }
    if (c == EOF) {
        fprintf(stderr, "Error: next_c: Unexpected EOF: %d\n", line);
        exit(1);
    }
    return c;
}

/* skips any white space from current position to next character*/
void skip_ws(FILE *json) {
    int c = next_c(json);
    while (isspace(c)) {
        c = next_c(json);
    }
    if (c == '\n')
        line--;         // we backed up to the previous line
    ungetc(c, json);    // move back one character (instead of fseek)
}

/* checks that the next character is d */
void expect_c(FILE* json, int d) {
    int c = next_c(json);
    if (c == d) return;
    fprintf(stderr, "Error: Expected '%c': %d\n", d, line);
    exit(1);
}

/* gets the next value from a file - This is *expected* to be a number */
double next_number(FILE* json) {
    double val;
    int res = fscanf(json, "%lf", &val);
    if (res == EOF) {
        fprintf(stderr, "Error: Expected a number but found EOF: %d\n", line);
        exit(1);
    }
    return val;
}

/* since we could use 0-255 or 0-1 or whatever, this function checks bounds */
int check_color_val(double v) {
    if (v < 0.0 || v > 1.0)
        return 0;
    return 1;
}

/* check bounds for colors in json light objects. These can be anything >= 0 */
int check_light_color_val(double v) {
    if (v < 0.0)
        return 0;
    return 1;
}

/* gets the next 3 values from FILE as vector coordinates */
double* next_vector(FILE* json) {
    double* v = malloc(sizeof(double)*3);
    skip_ws(json);
    expect_c(json, '[');
    skip_ws(json);
    v[0] = next_number(json);
    skip_ws(json);
    expect_c(json, ',');
    skip_ws(json);
    v[1] = next_number(json);
    skip_ws(json);
    expect_c(json, ',');
    skip_ws(json);
    v[2] = next_number(json);
    skip_ws(json);
    expect_c(json, ']');
    return v;
}

/* Checks that the next 3 values in the FILE are valid rgb numbers */
double* next_color(FILE* json, boolean is_rgb) {
    double* v = malloc(sizeof(double)*3);
    skip_ws(json);
    expect_c(json, '[');
    skip_ws(json);
    v[0] = next_number(json);
    skip_ws(json);
    expect_c(json, ',');
    skip_ws(json);
    v[1] = next_number(json);
    skip_ws(json);
    expect_c(json, ',');
    skip_ws(json);
    v[2] = next_number(json);
    skip_ws(json);
    expect_c(json, ']');
    // check that all values are valid
    if (is_rgb) {
        if (!check_color_val(v[0]) ||
            !check_color_val(v[1]) ||
            !check_color_val(v[2])) {
            fprintf(stderr, "Error: next_color: rgb value out of range: %d\n", line);
            exit(1);
        }
    }
    else {
        if (!check_light_color_val(v[0]) ||
            !check_light_color_val(v[1]) ||
            !check_light_color_val(v[2])) {
            fprintf(stderr, "Error: next_color: light value out of range: %d\n", line);
            exit(1);
        }

    }
    return v;
}

/* grabs a string wrapped in quotes from FILE */
char* parse_string(FILE *json) {
    skip_ws(json);
    int c = next_c(json);
    if (c != '"') {
        fprintf(stderr, "Error: Expected beginning of string but found '%c': %d\n", c, line);
        exit(1); // not a string
    }
    c = next_c(json); // should be first char in the string
    char buffer[128]; // strings are gauranteed to be 128 or less
    int i = 0;
    while (c != '"') {
        if (isspace(c)) {
            continue;
        }
        buffer[i] = c;
        i++;
        c = next_c(json);
    }
    buffer[i] = 0;
    return strdup(buffer); // returns a malloc'd version of buffer
}

/**
 * Reads all scene info from a json file and stores it in the global object
 * array. This does a lot of work...It checks for specific values and keys in
 * the file and places the values into the appropriate portion of the current
 * object.
 * @param json file handler with ASCII json data
 */
void read_json(FILE *json) {
    //read in data from file
    // expecting square bracket but we need to get rid of whitespace
    skip_ws(json);

    // find beginning of the list
    int c  = next_c(json);
    if (c != '[') {
        fprintf(stderr, "Error: read_json: JSON file must begin with [\n");
        exit(1);
    }
    skip_ws(json);
    c = next_c(json);

    // check if file empty
    if (c == ']' || c == EOF) {
        fprintf(stderr, "Error: read_json: Empty json file\n");
        exit(1);
    }
    skip_ws(json);

    int obj_counter = 0;
    int light_counter = 0;
    int obj_type;
    boolean not_done = true;
    // find the objects
    while (not_done) {
        //c  = next_c(json);
        if (obj_counter > MAX_OBJECTS) {
            fprintf(stderr, "Error: read_json: Number of objects is too large: %d\n", line);
            exit(1);
        }
        if (c == ']') {
            fprintf(stderr, "Error: read_json: Unexpected ']': %d\n", line);
            fclose(json);
            exit(1);
        }
        if (c == '{') {     // found an object
            skip_ws(json);
            char *key = parse_string(json);
            if (strcmp(key, "type") != 0) {
                fprintf(stderr, "Error: read_json: First key of an object must be 'type': %d\n", line);
                exit(1);
            }
            skip_ws(json);
            // get the colon
            expect_c(json, ':');
            skip_ws(json);

            char *type = parse_string(json);
            if (strcmp(type, "camera") == 0) {
                obj_type = CAMERA;
                objects[obj_counter].type = CAMERA;
            }
            else if (strcmp(type, "sphere") == 0) {
                obj_type = SPHERE;
                objects[obj_counter].type = SPHERE;
            }
            else if (strcmp(type, "plane") == 0) {
                obj_type = PLANE;
                objects[obj_counter].type = PLANE;
            }
            else if (strcmp(type, "light") == 0) {
                obj_type = LIGHT;
            }
            else {
                exit(1);
            }

            skip_ws(json);

            while (true) {
                //  , }
                c = next_c(json);
                if (c == '}') {
                    // stop parsing this object
                    break;
                }
                else if (c == ',') {
                    // read another field
                    skip_ws(json);
                    char* key = parse_string(json);
                    skip_ws(json);
                    expect_c(json, ':');
                    skip_ws(json);
                    if (strcmp(key, "width") == 0) {
                        if (obj_type != CAMERA) {
                            fprintf(stderr, "Error: read_json: Width cannot be set on this type: %d\n", line);
                            exit(1);
                        }
                        double temp = next_number(json);
                        if (temp <= 0) {
                            fprintf(stderr, "Error: read_json: width must be positive: %d\n", line);
                            exit(1);
                        }

                        objects[obj_counter].camera.width = temp;

                    }
                    else if (strcmp(key, "height") == 0) {
                        if (obj_type != CAMERA) {
                            fprintf(stderr, "Error: read_json: Height cannot be set on this type: %d\n", line);
                            exit(1);
                        }
                        double temp = next_number(json);
                        if (temp <= 0) {
                            fprintf(stderr, "Error: read_json: height must be positive: %d\n", line);
                            exit(1);
                        }
                        objects[obj_counter].camera.height = temp;
                    }
                    else if (strcmp(key, "radius") == 0) {
                        if (obj_type != SPHERE) {
                            fprintf(stderr, "Error: read_json: Radius cannot be set on this type: %d\n", line);
                            exit(1);
                        }
                        double temp = next_number(json);
                        if (temp <= 0) {
                            fprintf(stderr, "Error: read_json: radius must be positive: %d\n", line);
                            exit(1);
                        }
                        objects[obj_counter].sphere.radius = temp;
                    }
                    else if (strcmp(key, "theta") == 0) {
                        if (obj_type != LIGHT) {
                            fprintf(stderr, "Error: read_json: Theta cannot be set on this type: %d\n", line);
                            exit(1);
                        }
                        double theta = next_number(json);
                        if (theta > 0.0) {
                            lights[light_counter].type = SPOTLIGHT;
                        }
                        else if (theta < 0.0) {
                            fprintf(stderr, "Error: read_json: theta must be >= 0: %d\n", line);
                            exit(1);
                        }
                        lights[light_counter].theta_deg = theta;
                    }
                    else if (strcmp(key, "radial-a0") == 0) {
                        if (obj_type != LIGHT) {
                            fprintf(stderr, "Error: read_json: Radial-a0 cannot be set on this type: %d\n", line);
                            exit(1);
                        }
                        double rad_a = next_number(json);
                        if (rad_a < 0) {
                            fprintf(stderr, "Error: read_json: radial-a0 must be positive: %d\n", line);
                            exit(1);
                        }
                        lights[light_counter].rad_att0 = rad_a;
                    }
                    else if (strcmp(key, "radial-a1") == 0) {
                        if (obj_type != LIGHT) {
                            fprintf(stderr, "Error: read_json: Radial-a1 cannot be set on this type: %d\n", line);
                            exit(1);
                        }
                        double rad_a = next_number(json);
                        if (rad_a < 0) {
                            fprintf(stderr, "Error: read_json: radial-a1 must be positive: %d\n", line);
                            exit(1);
                        }
                        lights[light_counter].rad_att1 = rad_a;
                    }
                    else if (strcmp(key, "radial-a2") == 0) {
                        if (obj_type != LIGHT) {
                            fprintf(stderr, "Error: read_json: Radial-a2 cannot be set on this type: %d\n", line);
                            exit(1);
                        }
                        double rad_a = next_number(json);
                        if (rad_a < 0) {
                            fprintf(stderr, "Error: read_json: radial-a2 must be positive: %d\n", line);
                            exit(1);
                        }
                        lights[light_counter].rad_att2 = rad_a;
                    }
                    else if (strcmp(key, "angular-a0") == 0) {
                        if (obj_type != LIGHT) {
                            fprintf(stderr, "Error: read_json: Angular-a0 cannot be set on this type: %d\n", line);
                            exit(1);
                        }
                        double ang_a = next_number(json);
                        if (ang_a < 0) {
                            fprintf(stderr, "Error: read_json: angular-a0 must be positive: %d\n", line);
                            exit(1);
                        }
                        lights[light_counter].ang_att0 = ang_a;
                    }
                    else if (strcmp(key, "color") == 0) {
                        if (obj_type != LIGHT) {
                            fprintf(stderr, "Error: Just plain 'color' vector can only be applied to a light object\n");
                            exit(1);
                        }
                        lights[light_counter].color = next_color(json, false);
                    }
                    else if (strcmp(key, "direction") == 0) {
                        if (obj_type != LIGHT) {
                            fprintf(stderr, "Error: Direction vector can only be applied to a light object\n");
                            exit(1);
                        }
                        lights[light_counter].type = SPOTLIGHT;
                        lights[light_counter].direction = next_vector(json);
                    }
                    else if (strcmp(key, "specular_color") == 0) {
                        if (obj_type == SPHERE)
                            objects[obj_counter].sphere.spec_color = next_color(json, true);
                        else if (obj_type == PLANE)
                            objects[obj_counter].plane.spec_color = next_color(json, true);
                        else {
                            fprintf(stderr, "Error: read_json: speculaor_color vector can't be applied here: %d\n", line);
                            exit(1);
                        }
                    }
                    else if (strcmp(key, "diffuse_color") == 0) {
                        if (obj_type == SPHERE)
                            objects[obj_counter].sphere.diff_color = next_color(json, true);
                        else if (obj_type == PLANE)
                            objects[obj_counter].plane.diff_color = next_color(json, true);
                        else {
                            fprintf(stderr, "Error: read_json: diffuse_color vector can't be applied here: %d\n", line);
                            exit(1);
                        }
                    }
                    else if (strcmp(key, "position") == 0) {
                        if (obj_type == SPHERE)
                            objects[obj_counter].sphere.position = next_vector(json);
                        else if (obj_type == PLANE)
                            objects[obj_counter].plane.position = next_vector(json);
                        else if (obj_type == LIGHT)
                            lights[light_counter].position = next_vector(json);
                        else {
                            fprintf(stderr, "Error: read_json: Position vector can't be applied here: %d\n", line);
                            exit(1);
                        }

                    }
                    else if (strcmp(key, "normal") == 0) {
                        if (obj_type != PLANE) {
                            fprintf(stderr, "Error: read_json: Normal vector can't be applied here: %d\n", line);
                            exit(1);
                        }
                        else
                            objects[obj_counter].plane.normal = next_vector(json);
                    }
                    else {
                        fprintf(stderr, "Error: read_json: '%s' not a valid object: %d\n", key, line);
                        exit(1);
                    }
                    skip_ws(json);
                }
                else {
                    fprintf(stderr, "Error: read_json: Unexpected value '%c': %d\n", c, line);
                    exit(1);
                }
            }
            skip_ws(json);
            c = next_c(json);
            if (c == ',') {
                // noop
                skip_ws(json);
            }
            else if (c == ']') {
                not_done = false;
            }
            else {
                fprintf(stderr, "Error: read_json: Expecting comma or ]: %d\n", line);
                exit(1);
            }
        }
        if (obj_type == LIGHT) {
            if (lights[light_counter].type == SPOTLIGHT) {
                if (lights[light_counter].direction == NULL) {
                    fprintf(stderr, "Error: read_json: 'spotlight' light type must have a direction: %d\n", line);
                    exit(1);
                }
                if (lights[light_counter].theta_deg == 0.0) {
                    fprintf(stderr, "Error: read_json: 'spotlight' light type must have a theta value: %d\n", line);
                    exit(1);
                }
            }
            light_counter++;
        }
        else {
            if (obj_type == SPHERE || obj_type == PLANE) {
                if (objects[obj_counter].sphere.spec_color == NULL) {
                    fprintf(stderr, "Error: read_json: object must have a specular color: %d\n", line);
                    exit(1);
                }
                if (objects[obj_counter].sphere.diff_color == NULL) {
                    fprintf(stderr, "Error: read_json: object must have a diffuse color: %d\n", line);
                    exit(1);
                }
            }
            if (obj_type == CAMERA) {
                if (objects[obj_counter].camera.width == 0) {
                    fprintf(stderr, "Error: read_json: camera must have a width: %d\n", line);
                    exit(1);
                }
                if (objects[obj_counter].camera.height == 0) {
                    fprintf(stderr, "Error: read_json: camera must have a height: %d\n", line);
                    exit(1);
                }
            }
            obj_counter++;
        }
        if (not_done)
            c = next_c(json);
    }
    fclose(json);
    nlights = light_counter;
    nobjects = obj_counter;
}

/**
 * initializes list of objects to be empty for each element
 */
void init_objects() {
    memset(objects, '\0', sizeof(objects));
}

/**
 * initializes list of lights to be empty for each element
 */
void init_lights() {
    memset(lights, '\0', sizeof(lights));
}

/* testing/debug functions */
void print_objects(object *obj) {
    int i = 0;
    while (i < MAX_OBJECTS && obj[i].type > 0) {
        printf("object type: %d\n", obj[i].type);
        if (obj[i].type == CAMERA) {
            printf("height: %lf\n", obj[i].camera.height);
            printf("width: %lf\n", obj[i].camera.width);
        }
            // TODO: add printing of diffuse colors as well
        else if (obj[i].type == SPHERE) {
            printf("color: %lf %lf %lf\n", obj[i].sphere.spec_color[0],
                   obj[i].sphere.spec_color[1],
                   obj[i].sphere.spec_color[2]);
            printf("position: %lf %lf %lf\n", obj[i].sphere.position[0],
                   obj[i].sphere.position[1],
                   obj[i].sphere.position[2]);
            printf("radius: %lf\n", obj[i].sphere.radius);
        }
        else if (obj[i].type == PLANE) {
            printf("color: %lf %lf %lf\n", obj[i].plane.spec_color[0],
                   obj[i].plane.spec_color[1],
                   obj[i].plane.spec_color[2]);
            printf("position: %lf %lf %lf\n", obj[i].plane.position[0],
                   obj[i].plane.position[1],
                   obj[i].plane.position[2]);
            printf("normal: %lf %lf %lf\n", obj[i].plane.normal[0],
                   obj[i].plane.normal[1],
                   obj[i].plane.normal[2]);
        }
        else {
            printf("unsupported value\n");
        }
        i++;
    }
    printf("end at i=%d\n", i);
}
