//
// Created by mkg on 10/13/2016.
//
#include <math.h>
#include "../include/illumination.h"
#include "../include/vector_math.h"
#include "../include/json.h"

/**
 * Clamps colors -- makes sure they are within a certain range. We don't want values outside of 0-1
 * @param color_val
 * @return the clamped color value
 */
double clamp(double color_val){
    if (color_val < 0)
        return 0;
    else if (color_val > 1)
        return 1;
    else
        return color_val;
}

/**
 * Calculates the diffuse reflection of an object and puts it into an RGB color
 * @param N - normal vector of the object (should be pre-normalized)
 * @param L - Vector from intersection point on object to the light (should be pre-normalized)
 * @param IL - Illumination level of the light. In essence the "color" of the light
 * @param KD - The diffuse color of the object
 * @param out_color - the resulting RGB color when we're done
 */
void calculate_diffuse(double *N, double *L, double *IL, double *KD, double *out_color) {
    // K_a*I_a should be added to the beginning of this whole thing, which is a constant and ambient light
    double n_dot_l = v3_dot(N, L);
    if (n_dot_l > 0) {
        double diffuse_product[3];
        diffuse_product[0] = KD[0] * IL[0];
        diffuse_product[1] = KD[1] * IL[1];
        diffuse_product[2] = KD[2] * IL[2];
        // multiply by n_dot_l and store in out_color
        v3_scale(diffuse_product, n_dot_l, out_color);
    }
    else {
        // would normally return K_a*I_a here...
        out_color[0] = 0;
        out_color[1] = 0;
        out_color[2] = 0;
    }
}

/**
 * Calculates the specular reflection of an object and puts in into an RGB color
 * @param ns - shininess constant
 * @param L - Vector from intersection of object to the light
 * @param R - Reflection vector of the L vector
 * @param N - Normal vector of the object (should be pre-normalized)
 * @param V - Vector from the intersection point on the object to the camera or previous object
 * @param KS - Object's specular color
 * @param IL - Illumination level of the light. In essence the "color" of the light
 * @param out_color - where we store the resulting RGB color value
 */
void calculate_specular(double ns, double *L, double *R, double *N, double *V, double *KS, double *IL, double *out_color) {
    double v_dot_r = v3_dot(V, R);
    double n_dot_l = v3_dot(N, L);
    if (v_dot_r > 0 && n_dot_l > 0) {
        double vr_to_the_ns = pow(v_dot_r, ns);
        double spec_product[3];
        spec_product[0] = KS[0] * IL[0];
        spec_product[1] = KS[1] * IL[1];
        spec_product[2] = KS[2] * IL[2];
        v3_scale(spec_product, vr_to_the_ns, out_color);
    }
    else {
        v3_zero(out_color);
    }
}

/**
 * Calculates the angular attenuation of light based on predefined theta value and direction
 * @param light - light struct that we care about
 * @param direction_to_object - direction vector from the light to the object
 * @return - returns the attenuation value
 */
double calculate_angular_att(Light *light, double direction_to_object[3]) {
    if (light->type != SPOTLIGHT)
        return 1.0;
    if (light->direction == NULL) {
        fprintf(stderr, "Error: calculate_angular_att: Can't have spotlight with no direction\n");
        exit(1);
    }
    normalize(light->direction);
    double theta_rad = light->theta_deg * (M_PI / 180.0);
    double cos_theta = cos(theta_rad);
    double vo_dot_vl = v3_dot(light->direction, direction_to_object);
    if (vo_dot_vl < cos_theta)
        return 0.0;
    return pow(vo_dot_vl, light->ang_att0);
}

/**
 * Calculates the radial attenuation of a light source
 * @param light - light struct that we care about
 * @param distance_to_light - distance from the object we're calculating this on to the light
 * @return - returns the attenuation value
 */
double calculate_radial_att(Light *light, double distance_to_light) {
    if (light->rad_att0 == 0 && light->rad_att1 == 0 && light->rad_att2 == 0) {
        fprintf(stdout, "WARNING: calculate_radial_att: Found all 0s for attenuation. Assuming default values of radial attenuation\n");
        light->rad_att2 = 1.0;
    }
    // if d_l == infinity, return 1
    if (distance_to_light > 99999999999999) return 1.0;

    double dl_sqr = sqr(distance_to_light);
    double denom = light->rad_att2 * dl_sqr + light->rad_att1 * distance_to_light + light->ang_att0;
    return 1.0 / denom;
}
