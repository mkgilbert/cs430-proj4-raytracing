//
// Created by mkg on 10/7/2016.
//

#include "../include/raytracer.h"
#include "../include/vector_math.h"
#include "../include/json.h"
#include "../include/illumination.h"

/* raycast.c - provides raycasting functionality */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SHININESS 20        // constant for shininess
#define MAX_REC_LEVEL 7     // maximum recursion level for raytracing

/* overall background color for the image */
V3 background_color = {0, 0, 0};

/**
 * Finds and gets the index in objects that has the camera width and height
 * @param objects - array of object types that represent the scene
 * @return int - non-negative if the object was found, -1 otherwise
 */
int get_camera(object *objects) {
    int i = 0;
    while (i < MAX_OBJECTS && objects[i].type != 0) {
        if (objects[i].type == CAMERA) {
            return i;
        }
        i++;
    }
    // no camera found in data
    return -1;
}

/**
 * colors the values of a pixel based on the color array that is passed in
 * @param color - array of 3 color values for r,g,b
 * @param row - which row the pixel is on
 * @param col - which column the pixel is on
 * @param img - image struct that allows for indexing the appropriate spot
 */
void set_pixel_color(double *color, int row, int col, image *img) {
    // fill in pixel color values
    // the color vals are stored as values between 0 and 1, so we need to adjust
    img->pixmap[row * img->width + col].r = (unsigned char)(MAX_COLOR_VAL * clamp(color[0]));
    img->pixmap[row * img->width + col].g = (unsigned char)(MAX_COLOR_VAL * clamp(color[1]));
    img->pixmap[row * img->width + col].b = (unsigned char)(MAX_COLOR_VAL * clamp(color[2]));
}

/** Tests for an intersection between a ray and a plane
 * @param Ro - 3d vector of ray origin
 * @param Rd - 3d vector of ray direction
 * @param Pos - 3d vector of the plane's position
 * @param Norm - 3d vector of the normal to the plane
 * @return - distance to the object if intersects, otherwise, -1
 */
double plane_intersect(Ray *ray, double *Pos, double *Norm) {
    normalize(Norm);
    // determine if plane is parallel to the ray
    double vd = v3_dot(Norm, ray->direction);

    if (fabs(vd) < 0.0001) return -1;

    double vector[3];
    v3_sub(Pos, ray->origin, vector);
    double t = v3_dot(vector, Norm) / vd;

    // no intersection
    if (t < 0.0)
        return -1;

    return t;
}

/**
 * Tests for an intersection between a ray and a sphere
 * @param Ro - 3d vector of ray origin
 * @param Rd - 3d vector of ray direction
 * @param C - 3d vector of the center of the sphere
 * @param r - radius of the sphere
 * @return - distance to the object if intersects, otherwise, -1
 */
double sphere_intersect(Ray *ray, double *C, double r) {
    double b, c;
    double vector_diff[3];
    //v3_sub(ray->direction, C, vector_diff);
    v3_sub(ray->origin, C, vector_diff);

    // calculate quadratic formula
    b = 2 * (ray->direction[0]*vector_diff[0] + ray->direction[1]*vector_diff[1] + ray->direction[2]*vector_diff[2]);
    c = sqr(vector_diff[0]) + sqr(vector_diff[1]) + sqr(vector_diff[2]) - sqr(r);

    // check that discriminant is <, =, or > 0
    double disc = sqr(b) - 4*c;
    double t;  // solutions
    if (disc < 0) {
        return -1; // no solution
    }
    disc = sqrt(disc);
    t = (-b - disc) / 2.0;
    if (t < 0.0)
        t = (-b + disc) / 2.0;

    if (t < 0.0)
        return -1;
    return t;
}

/**
 * gets the reflection vector of a vector going in "direction" direction. It uses "position" to help determine
 * the normal if the object is a sphere
 * @param direction - direction vector that we are reflecting
 * @param position  - position where the direction vector is hitting the object (so we can determine the normal vector)
 * @param obj_index - index into objects array. i.e. the current object we are reflecting off of
 * @param reflection - the resulting reflection vector
 */
void reflection_vector(V3 direction, V3 position, int obj_index, V3 reflection) {
    V3 normal;
    if (objects[obj_index].type == PLANE) {
        v3_copy(objects[obj_index].plane.normal, normal);
    }
    else if (objects[obj_index].type == SPHERE) {
        v3_sub(position, objects[obj_index].sphere.position, normal);
    }
    normalize(normal);
    v3_reflect(direction, normal, reflection);
}

/**
 * Shoots out a ray to check for the closest object intersection
 * @param ray - the ray we are shooting out to find an intersection with
 * @param self_index - if < 0, ignore this. If >= 0, it is the index of the object we are getting distance FROM
 * @param max_distance - This is the maximum distance we care to check. e.g. distance to a light source
 * @param ret_index - the index in objects array of the closest object we intersected
 * @param ret_best_t - the distance of the closest object
 */
void shoot(Ray *ray, int self_index, double max_distance, int *ret_index, double *ret_best_t) {
    int best_o = -1;
    double best_t = INFINITY;
    for (int i=0; objects[i].type != 0; i++) {
        // if self_index was passed in as > 0, we must ignore object i because we are checking distance to another
        // object from the one at self_index.
        if (self_index == i) continue;

        // we need to run intersection test on each object
        double t = 0;
        switch(objects[i].type) {
            case 0:
                printf("no object found\n");
                break;
            case CAMERA:
                break;
            case SPHERE:
                t = sphere_intersect(ray, objects[i].sphere.position,
                                     objects[i].sphere.radius);
                break;
            case PLANE:
                t = plane_intersect(ray, objects[i].plane.position,
                                    objects[i].plane.normal);
                break;
            default:
                // Error
                exit(1);
        }
        if (max_distance != INFINITY && t > max_distance)
            continue;
        if (t > 0 && t < best_t) {
            best_t = t;
            best_o = i;
        }
    }
    (*ret_index) = best_o;
    (*ret_best_t) = best_t;
}

/**
 * This determines a color shade directly, determining the attenuation of a given light along with the diffuse
 * and specular colors of the object.
 * @param ray - the ray coming into the object at objects[obj_index]
 * @param obj_index - index into the objects array. i.e. the current object we are determining the color of
 * @param position - The current vector position that the ray has intersected with the object
 * @param light - The specific light object in the scene that we are using to determine the shade of this object
 * @param max_dist - The furthest distance from the object we should be allowing. This is the distance from the current
 * object to the light object position
 * @param color - This is the final color value when the function is complete
 */
void direct_shade(Ray *ray, int obj_index, double position[3], Light *light, double max_dist, double color[3]) {
    double normal[3];
    double obj_diff_color[3];
    double obj_spec_color[3];

    // find normal and color
    if (objects[obj_index].type == PLANE) {
        v3_copy(objects[obj_index].plane.normal, normal);
        v3_copy(objects[obj_index].plane.diff_color, obj_diff_color);
        v3_copy(objects[obj_index].plane.spec_color, obj_spec_color);
    } else if (objects[obj_index].type == SPHERE) {
        // find normal of our current intersection on the sphere
        v3_sub(ray->origin, objects[obj_index].sphere.position, normal);
        // copy the colors into temp variables
        v3_copy(objects[obj_index].sphere.diff_color, obj_diff_color);
        v3_copy(objects[obj_index].sphere.spec_color, obj_spec_color);
    } else {
        fprintf(stderr, "Error: shade: Trying to shade unsupported type of object\n");
        exit(1);
    }
    normalize(normal);
    // find light, reflection and camera vectors
    double L[3];
    double R[3];
    double V[3];
    v3_copy(ray->direction, L);
    normalize(L);
    v3_reflect(L, normal, R);
    v3_copy(position, V);
    double diffuse[3];
    double specular[3];
    v3_zero(diffuse);
    v3_zero(specular);
    calculate_diffuse(normal, L, light->color, obj_diff_color, diffuse);
    calculate_specular(SHININESS, L, R, normal, V, obj_spec_color, light->color, specular);

    // calculate the angular and radial attenuation
    double fang;
    double frad;
    // get the vector from the object to the light
    double light_to_obj_dir[3];
    v3_copy(L, light_to_obj_dir);
    v3_scale(light_to_obj_dir, -1, light_to_obj_dir);

    fang = calculate_angular_att(light, light_to_obj_dir);
    frad = calculate_radial_att(light, max_dist);
    color[0] += frad * fang * (specular[0] + diffuse[0]);
    color[1] += frad * fang * (specular[1] + diffuse[1]);
    color[2] += frad * fang * (specular[2] + diffuse[2]);
}

/**
 * @param ray - original ray -- starting point for testing shade
 * @param obj_index  - index of the current object we are running shade on
 * @param t - distance to the object
 * @param color - this will be the output color after shade calculations are done
 * @param rec_level - This is the current level of recursion we are on
 */
void shade(Ray *ray, int obj_index, double t, int rec_level, double color[3]) {
    // check that we haven't done too many recursions
    if (rec_level > MAX_REC_LEVEL) {
        // return black for color
        color[0] = 0;
        color[1] = 0;
        color[2] = 0;
        return;
    }

    double new_origin[3] = {0, 0, 0};
    double new_dir[3] = {0, 0, 0};

    // find new ray origin
    if (ray == NULL) {
        fprintf(stderr, "Error: shade: Ray had no data\n");
        exit(1);
    }
    v3_scale(ray->direction, t, new_origin);
    v3_add(new_origin, ray->origin, new_origin);

    Ray ray_new = {
            .origin = {new_origin[0], new_origin[1], new_origin[2]},
            .direction = {new_dir[0], new_dir[1], new_dir[2]}
    };

    // get nearest object based on reflection vector of ray->direction
    V3 reflection = {0, 0, 0};
    V3 obj_to_view = {0, 0, 0};
    v3_scale(ray->direction, -1, obj_to_view);
    reflection_vector(obj_to_view, ray_new.origin, obj_index, reflection);   // stores reflection of the new origin in "reflection"

    // create temp variables to use for recursively shading
    int best_o;     // index of closest object
    double best_t;  // distance of closest object
    Ray ray_reflected = {
            .origin = {new_origin[0], new_origin[1], new_origin[2]},
            .direction = {reflection[0], reflection[1], reflection[2]}
    };

    // shoot new reflection vector out as a new ray, to check if there is an intersection with another object
    shoot(&ray_reflected, obj_index, INFINITY, &best_o, &best_t);

    if (best_o == -1) { // there were no objects that we intersected with
        /*color[0] = 0;
        color[1] = 0;
        color[2] = 0;*/
    }
    else {  // we had an intersection, so we need to recursively shade...
        double reflection_color[3] = {0, 0, 0};
        shade(&ray_reflected, best_o, best_t, rec_level+1, reflection_color);
        Light light;
        light.direction = malloc(sizeof(V3));
        light.color = malloc(sizeof(double)*3);
        v3_scale(reflection, -1, light.direction);
        light.color[0] = reflection_color[0];
        light.color[1] = reflection_color[1];
        light.color[2] = reflection_color[2];
        direct_shade(ray, obj_index, ray_reflected.direction, &light, INFINITY, color);
        free(light.direction);
        free(light.color);
    }
    for (int i=0; i<nlights; i++) {
        // find new ray direction

        v3_sub(lights[i].position, ray_new.origin, ray_new.direction);
        double distance_to_light = v3_len(ray_new.direction);
        normalize(ray_new.direction);

        // new check new ray for intersections with other objects
        shoot(&ray_new, obj_index, distance_to_light, &best_o, &best_t);

        if (best_o == -1) { // this means there was no object in the way between the current one and the light
            direct_shade(&ray_new, obj_index, ray->direction, &lights[i], distance_to_light, color);
        }
        // there was an object in the way, so we don't do anything. It's shadow
    }
}

/**
 * Shoots out rays over a viewplane of dimensions stored in img and looks through
 * the array of objects for an intersection for each pixel.
 * @param img - image data (width, height, pixmap...)
 * @param cam_width - camera width
 * @param cam_height - camera height
 * @param objects - array of objects in the scene
 */
void raycast_scene(image *img, double cam_width, double cam_height, object *objects) {
    // loop over all pixels and test for intesections with objects.
    // store results in pixmap
    double vp_pos[3] = {0, 0, 1};   // view plane position
    double point[3] = {0, 0, 0};    // point on viewplane where intersection happens

    double pixheight = (double)cam_height / (double)img->height;
    double pixwidth = (double)cam_width / (double)img->width;

    Ray ray = {
            .origin = {0, 0, 0},
            .direction = {0, 0, 0}
    };

    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            v3_zero(ray.origin);
            v3_zero(ray.direction);
            point[0] = vp_pos[0] - cam_width/2.0 + pixwidth*(j + 0.5);
            point[1] = -(vp_pos[1] - cam_height/2.0 + pixheight*(i + 0.5));
            point[2] = vp_pos[2];    // set intersecting point Z to viewplane Z
            normalize(point);   // normalize the point
            // store normalized point as our ray direction
            v3_copy(point, ray.direction);
            double color[3] = {0, 0, 0};

            int best_o;     // index of 'best' or closest object
            double best_t;  // closest distance
            shoot(&ray, -1, INFINITY, &best_o, &best_t);

            // set ambient color
            if (best_t > 0 && best_t != INFINITY && best_o != -1) {// there was an intersection
                shade(&ray, best_o, best_t, 0, color);
                set_pixel_color(color, i, j, img);
            }
            else {
                set_pixel_color(background_color, i, j, img);
            }
        }
    }
}