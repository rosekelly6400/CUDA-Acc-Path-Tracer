CUDA Path Tracer
================

**University of Pennsylvania, CIS 565: GPU Programming and Architecture, Project 3**

* (TODO) YOUR NAME HERE
* Tested on: (TODO) Windows 22, i7-2222 @ 2.22GHz 22GB, GTX 222 222MB (Moore 2222 Lab)

### (TODO: Your README)

*DO NOT* leave the README to the last minute! It is a crucial part of the
project, and we will not be able to grade you without a good README.


Sources:
tinygltf : https://github.com/syoyo/tinygltf/tree/release
gltf reference guide by khronos: https://www.khronos.org/files/gltf20-reference-guide.pdf 
sample glTF models: https://github.com/KhronosGroupArchives/glTF-Sample-Models
sample obj models (converted to glTF in blender): https://github.com/alecjacobson/common-3d-test-models
PBR book: https://pbr-book.org/3ed-2018

Core Features
- diffuse material and perfectly specular material
    - IMAGE diffuse and specular right next to each other
- sorting rays by material
    - GRAPH
- stream compaction to terminate dead rays
    - GRAPH
- stochastic sampled antialiasing
    - IMAGE compare pixelation

CORE FEATURES TOGGLES
- SORT_RAYS_BY_MATERIAL
- RAY_STREAM_COMPACTION
- ANTI_ALIASING
- USE_BVH_TREE
- USE_VERT_NORMALS
- USE_DEPTH_OF_FIELD

PART 2 FEATURES

- BVH TREE
    - GRAPH comparing
- GLTF File Reading
    - IMAGE some model with and without normals
- depth of field 
    - image of line of suzannes in hallway


Scenes
    - line of suzanne's
    - cows looking at reflective ball
    - 