cs430-proj3-illumination
=========================
Applies basic shading and lighting techniques to spheres and planes. Supports multiple light sources
and spotlights as well as pointlights. 

## Installation ##
The steps below are what is required to install the program on different architectures. Note that this project was built
with Clion by jetbrains, so cmake is a dependency to build the program.

*Note: All commands should be run from the project root directory*

### Linux ###
```
$ export LDFLAGS="-lm"
$ cmake .
$ make
```
### Mac OSX  and Windows ###
**Note: Must have cmake version >= 3.3.2 installed**
```
$ cmake .
$ make
```

## Running the program ##

#### Mac OSX and Linux ####
`$ ./cs430-proj3-illumination <width> <height> <input.json> <output>`

#### Windows ####
`> cs430-proj3-illumination.exe <width> <height> <input.json> <output>`