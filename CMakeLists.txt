cmake_minimum_required(VERSION 3.3.2)
project(raytrace)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

set(SOURCE_FILES src/main.c src/raytracer.c include/raytracer.h src/ppmrw.c include/ppmrw.h include/vector_math.h src/json.c include/json.h include/base.h src/illumination.c include/illumination.h)
add_executable(raytrace ${SOURCE_FILES} src/illumination.c include/illumination.h)