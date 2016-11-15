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
double sphere_intersect(Ray *ray, double *C, double r, boolean *in_sphere) {
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
    *(in_sphere) = false;
    if (t < 0.0) {
        *(in_sphere) = true;
        t = (-b + disc) / 2.0;
    }
    if (t < 0.0)
        return -1;
    return t;
}

void normal_vector(int obj_index, V3 position, V3 normal) {
    if (objects[obj_index].type == PLANE) {
        v3_copy(objects[obj_index].plane.normal, normal);
    }
    else if (objects[obj_index].type == SPHERE) {
        v3_sub(position, objects[obj_index].sphere.position, normal);
    }
    else {
        fprintf(stderr, "Error: normal_vector: This object type does not have a normal vector\n");
    }
}

double get_reflectivity(int obj_index) {
    if (objects[obj_index].type == PLANE) {
        return objects[obj_index].plane.reflect;
    }
    else if (objects[obj_index].type == SPHERE) {
        return objects[obj_index].sphere.reflect;
    }
    else {
        fprintf(stderr, "Error: get_reflectivity: Specified object does not have a reflect property\n");
        return -1;
    }
}

double get_refractivity(int obj_index) {
    if (objects[obj_index].type == PLANE) {
        return objects[obj_index].plane.refract;
    }
    else if (objects[obj_index].type == SPHERE) {
        return objects[obj_index].sphere.refract;
    }
    else {
        fprintf(stderr, "Error: get_refractivity: Specified object does not have a refract property\n");
        return -1;
    }
}

double get_ior(int obj_index) {
    double ior;
    if (objects[obj_index].type == PLANE) {
        ior = objects[obj_index].plane.ior;
    }
    else if (objects[obj_index].type == SPHERE) {
        ior = objects[obj_index].sphere.ior;
    }
    else {
        fprintf(stderr, "Error: get_ior: Specified object does not have an ior property\n");
        exit(1);
    }
    if (fabs(ior) < 0.0001)
        return 1;
    else
        return ior;
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
    normal_vector(obj_index, position, normal);
    normalize(normal);
    v3_reflect(direction, normal, reflection);
}

void refraction_vector(V3 direction, V3 position, int obj_index, double ext_ior, V3 refracted_vector, boolean *in_sphere) {
    // initializations and variables setup
    V3 dir, pos;
    v3_copy(direction, dir);
    v3_copy(position, pos);
    normalize(dir);
    normalize(pos);
    double int_ior = get_ior(obj_index);

    // This only works for this project...Assume that there are no objects intersecting other objects. Check if we are
    // already inside of a sphere. If we are, then the next ior will be 1 (air)
    if ((*in_sphere) == true)
        int_ior = 1;

    V3 normal, a, b;

    // find normal vector of current object
    normal_vector(obj_index, pos, normal);
    normalize(normal);

    // create coordinate frame with a and b, where b is tangent to the object intersection
    v3_cross(normal, dir, a);
    normalize(a);
    v3_cross(a, normal, b);

    // find transmission vector angle and direction
    double sin_theta = v3_dot(dir, b);
    double sin_phi = (ext_ior / int_ior) * sin_theta;
    double cos_phi = sqrt(1 - sqr(sin_phi));
    v3_scale(normal, -1*cos_phi, normal);
    v3_scale(b, sin_phi, b);
    v3_add(normal , b, refracted_vector);
}

/**
 * Shoots out a ray to check for the closest object intersection
 * @param ray - the ray we are shooting out to find an intersection with
 * @param self_index - if < 0, ignore this. If >= 0, it is the index of the object we are getting distance FROM
 * @param max_distance - This is the maximum distance we care to check. e.g. distance to a light source
 * @param ret_index - the index in objects array of the closest object we intersected
 * @param ret_best_t - the distance of the closest object
 */
void shoot(Ray *ray, int self_index, double max_distance, int *ret_index, double *ret_best_t, boolean *ret_in_sphere) {
    int best_o = -1;
    boolean in_sphere = false; // tells us if we are inside the sphere
    double best_t = INFINITY;
    for (int i=0; objects[i].type != 0; i++) {
        // if self_index was passed in as > 0, we must ignore object i because we are checking distance to another
        // object from the one at self_index.
        if (self_index == i) continue;

        // we need to run intersection test on each object
        double t = 0;
        in_sphere = false;
        switch(objects[i].type) {
            case 0:
                printf("no object found\n");
                break;
            case CAMERA:
                break;
            case SPHERE:
                t = sphere_intersect(ray, objects[i].sphere.position,
                                     objects[i].sphere.radius, &in_sphere);
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
    (*ret_in_sphere) = in_sphere;
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
    scale_color(diffuse, 0, diffuse);
    scale_color(specular, 0, specular);
    calculate_diffuse(normal, L, light->color, obj_diff_color, diffuse);
    calculate_specular(SHININESS, L, R, normal, V, obj_spec_color, light->color, specular);

    // calculate the angular and radial attenuation
    double fang;
    double frad;
    // get the vector from the object to the light
    double light_to_obj_dir[3];
    v3_copy(L, light_to_obj_dir);
    v3_scale(light_to_obj_dir, -1, light_to_obj_dir);

    if (light->type == OBJECT) { // the light source is a reflection off of another object, so don't calculate attenuation
        fang = 1;
        frad = 1;
    }
    else {
        fang = calculate_angular_att(light, light_to_obj_dir);
        frad = calculate_radial_att(light, max_dist);
    }
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
void shade(Ray *ray, int obj_index, double t, double curr_ior, int rec_level, double color[3], boolean *in_sphere) {
    // check that we haven't done too many recursions
    if (rec_level > MAX_REC_LEVEL) { // base case, reached max number of recursions
        // return black for color
        scale_color(color, 0, color);
        return;
    }
    if (obj_index == -1) {  // base case, no intersecting object had been found, so return black
        scale_color(color, 0, color);
        return;
    }
    if (ray == NULL) {
        fprintf(stderr, "Error: shade: Ray had no data\n");
        exit(1);
    }

    double new_origin[3] = {0, 0, 0};
    double new_dir[3] = {0, 0, 0};

    // find new ray origin
    v3_scale(ray->direction, t, new_origin);
    v3_add(new_origin, ray->origin, new_origin);

    Ray ray_new = {
            .origin = {new_origin[0], new_origin[1], new_origin[2]},
            .direction = {new_dir[0], new_dir[1], new_dir[2]}
    };

    // get nearest object based on reflection vector of ray->direction
    V3 reflection = {0, 0, 0};
    V3 refraction = {0, 0, 0};
    normalize(ray->direction);
    reflection_vector(ray->direction, ray_new.origin, obj_index, reflection);
    refraction_vector(ray->direction, ray_new.origin, obj_index, curr_ior, refraction, in_sphere);

    // create temp variables to use for recursively shading
    int best_refl_o;     // index of closest reflected object
    double best_refl_t;  // distance of closest reflected object
    int best_refr_o;     // index of closest refracted object
    double best_refr_t;  // distance of closest refracted object


    Ray ray_reflected = {
            .origin = {new_origin[0], new_origin[1], new_origin[2]},
            .direction = {reflection[0], reflection[1], reflection[2]}
    };
    Ray ray_refracted = {
            .origin = {new_origin[0], new_origin[1], new_origin[2]},
            .direction = {refraction[0], refraction[1], refraction[2]}
    };

    // offset the new origin of each ray by just a little in the direction of the ray, so we can avoid running into the same object again
    V3 offset = {0, 0, 0};
    v3_scale(ray_reflected.direction, 0.01, offset);
    v3_add(ray_reflected.origin, offset, ray_reflected.origin);
    v3_zero(offset);
    v3_scale(ray_refracted.direction, 0.01, offset);
    v3_add(ray_refracted.origin, offset, ray_refracted.origin);

    normalize(ray_reflected.direction);
    normalize(ray_refracted.direction);

    // shoot new reflection vector out as a new ray, to check if there is an intersection with another object
    shoot(&ray_reflected, -1, INFINITY, &best_refl_o, &best_refl_t, in_sphere);

    // we only want to shoot and possibly hit the same object we are currently on if it is a sphere, not a plane
    if (objects[obj_index].type == PLANE)
        shoot(&ray_refracted, -1, INFINITY, &best_refr_o, &best_refr_t, in_sphere);
    else
        shoot(&ray_refracted, -1, INFINITY, &best_refr_o, &best_refr_t, in_sphere);

    if (best_refl_o == -1 && best_refr_o == -1) { // there were no objects that we intersected with
        scale_color(color, 0, color);
    }
    else {  // we had an intersection, so we need to recursively shade...
        double reflection_color[3] = {0, 0, 0};
        double refraction_color[3] = {0, 0, 0};
        double reflect_constant = get_reflectivity(obj_index);
        double refract_constant = get_refractivity(obj_index);
        double refr_ior = 1;     // ior of closest object based on refraction vector
        double refl_ior = 1;     // ior of closest object based on reflection vector

        // create temp light object to hold object light reflection information
        Light refl_light;
        refl_light.type = OBJECT;
        refl_light.direction = malloc(sizeof(V3));
        refl_light.color = malloc(sizeof(double) * 3);
        Light refr_light;
        refr_light.type = OBJECT;
        refr_light.direction = malloc(sizeof(V3));
        refr_light.color = malloc(sizeof(double) * 3);

        // testing without refraction...
        //best_refr_o = -1;
        // end

        if (best_refl_o >= 0) {
            // recursively shade based on reflection
            refl_ior = get_ior(best_refl_o);
            shade(&ray_reflected, best_refl_o, best_refl_t, refl_ior, rec_level+1, reflection_color, in_sphere);
            v3_scale(reflection_color, reflect_constant, reflection_color);

            v3_scale(reflection, -1, refl_light.direction);
            copy_color(reflection_color, refl_light.color);

            // set the new ray direction based on this temp "light" object
            // first find the 3d position of the intersection with the "light" object
            v3_scale(ray_reflected.direction, best_refl_t, ray_reflected.direction);

            /****** changed this from v3_add to v3_sub and all of a sudden got full reflections *****/
            v3_sub(ray_reflected.direction, ray_new.origin, ray_new.direction);
            normalize(ray_new.direction);

            direct_shade(&ray_new, obj_index, ray->direction, &refl_light, INFINITY, color); // this version allows reflections to work
        }
        if (best_refr_o >= 0) {
            refr_ior = get_ior(best_refr_o);
            // recursively shade based on refraction
            shade(&ray_refracted, best_refr_o, best_refr_t, refr_ior, rec_level+1, refraction_color, in_sphere);
            v3_scale(refraction_color, refract_constant, refraction_color);

            v3_scale(refraction, -1, refr_light.direction);
            copy_color(refraction_color, refr_light.color);

            // set the new ray direction based on this temp "light" object
            // first find the 3d position of the intersection with the "light" object
            v3_scale(ray_refracted.direction, best_refr_t, ray_refracted.direction);

            /****** changed this from v3_add to v3_sub and all of a sudden got full reflections *****/
            v3_sub(ray_refracted.direction, ray_new.origin, ray_new.direction);
            normalize(ray_new.direction);

            direct_shade(&ray_new, obj_index, ray->direction, &refr_light, INFINITY, color); // this version allows reflections to work
        }

        // now add what is left of the original color of the object to the current intersection point
        if (reflect_constant == -1)
            reflect_constant = 0;
        if (refract_constant == -1)
            refract_constant = 0;
        // only shade the object with its natural color if it should be visible. If both constants are 0, then it should not be visible
        if (fabs(refract_constant) < 0.00001 && fabs(reflect_constant) < 0.00001) {
            copy_color(background_color, color);
        }
        else {
            double color_diff = 1.0 - reflect_constant - refract_constant;
            if (fabs(color_diff) < 0.0001) // account for numbers that are really close to 0, but still negative
                color_diff = 0;
            double obj_color[3] = {0, 0, 0};
            copy_color(objects[obj_index].plane.diff_color, obj_color);
            scale_color(obj_color, color_diff, obj_color);
            color[0] += obj_color[0];
            color[1] += obj_color[1];
            color[2] += obj_color[2];
        }

        //cleanup
        free(refl_light.direction);
        free(refl_light.color);
        free(refr_light.direction);
        free(refr_light.color);
    }

    for (int i=0; i<nlights; i++) {
        // find new ray direction
        int best_o;
        double best_t;
        v3_zero(ray_new.direction);
        v3_sub(lights[i].position, ray_new.origin, ray_new.direction);
        double distance_to_light = v3_len(ray_new.direction);
        normalize(ray_new.direction);

        // new check new ray for intersections with other objects
        shoot(&ray_new, obj_index, distance_to_light, &best_o, &best_t, in_sphere);

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
            boolean in_sphere = false;
            shoot(&ray, -1, INFINITY, &best_o, &best_t, &in_sphere);

            // Test pixel
            if (i == 330 && j == 490) {
                printf("found test pixel\n");
            }
            if (best_t > 0 && best_t != INFINITY && best_o != -1) {// there was an intersection
                shade(&ray, best_o, best_t, 1, 0, color, &in_sphere);
                set_pixel_color(color, i, j, img);
            }
            else {
                set_pixel_color(background_color, i, j, img);
            }
        }
    }
}