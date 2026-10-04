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
- Basic monte carlo path tracer and diffuse material and perfectly specular material
    - IMAGE diffuse and specular right next to each other
I implemented basic path tracing and diffuse and perfectly specular materials.
- sorting rays by material
    - GRAPH
I also implemented a feature to sort paths by material type before processing their bsdf and pdf. In theory this should improve performance by reducing thread divergence since different materials take different paths in the shade and scatter ray kernels and this slows performance within warps since if/else statements are serialized in a warp. Grouping paths of the same material in the buffer should make most warps only have one type of material, thus saving time by not having to run multiple if/else paths in serial execution. In practice this actually made my path tracer perform worse. This is likely because the saved time was minimal due to only having two materials implemented, making the overhead of running thrust's sort more costly than the saved time from less thread divergence.
- stream compaction to terminate dead rays
    - GRAPH
I implemented stream compaction to remove "dead" paths that no longer would contribute to the image, and thus would be wasting resources to allocate threads for. I used thrust's remove_if function to remove any paths that had no more remaining bounces (and used the remaining bounces variable in my pathtracing code to set a path as terminated in the case of it hitting nothing or a light). 
- stochastic sampled antialiasing
    - IMAGE compare pixelation
I implemented stochastic sampled antialiasing as described in the "stochastic sampling" section of this page: https://paulbourke.net/miscellaneous/raytracing/. My implementation splits each pixel up into 16 squares and as the pathtracer iterates, the rays cast from the camera through each pixel also iterate through the 16 sub-pixel squares by shooting a ray through a random location in that iteration's sub-pixel square. The result of jittering the ray is that the color of each overall pixel is the combination of random rays through the 16 sub-pixels. Harsh aliasing occurs because without this method the ray hits the same location in the scene each time on the first bounce even if the pixel contains multiple objects, like an edge of an object, which lead to the pixelated edges associated with aliasing. This method allows the pixel to reflect the combined colors of multiple objects contained in the pixel, creating a smoother, blurred edge instead of harsh pixelation.


CORE FEATURES TOGGLES
- SORT_RAYS_BY_MATERIAL
- RAY_STREAM_COMPACTION
- ANTI_ALIASING
- USE_BVH_TREE
- USE_VERT_NORMALS
- USE_DEPTH_OF_FIELD

PART 2 FEATURES

- GLTF File Reading
    - IMAGE some model with and without normals
 I added glTF file loading as a feature using the tinygltf library. This essentially consisted of loading the file using the library and accessing the vertex data for the mesh and indices indicating what order to use the vertex data to create triangles. I then used those triangles to create the BVH tree detailed below. The file reading and triangle construction is done on the CPU.
- BVH TREE
    - GRAPH comparing
I implemented a BVH tree acceleration structure based on the notes in the Physically Based Rendering book here: https://pbr-book.org/3ed-2018/Primitives_and_Intersection_Acceleration/Bounding_Volume_Hierarchies. This essentially consists of building a bounding volume around all of the triangles and then splitting the triangles into groups and sub groups and so on, each with their own bounding volume to create a hierarchy of bounding volumes the program can traverse to more quickly test intersection. 
I split the groups based on whether or not the centers of the triangles were above or below the midpoint of the longest axis of the bounding volume encompassing them.
Intersection for a ray is tested using a BVH tree by first testing if the ray hits the highest level bounding volume and if so it tests the bounding volume's two children. This continues down the tree to the "leaves" which are the actual triangles. This should reduce the amount of intersection tests needed to be done since instead of brute force checking intersection with every triangle, whole groups of triangles can be ignored if the ray doesn't intersect with their bounding volume. 
The BVH tree construction was done on the CPU and then buffered to the GPU, and the traversal is done on the GPU as part of the rest of the GPU pathtracing code.
- depth of field 
    - image of line of suzannes in hallway
I also added a depth of field effect. I jitter the ray in similar way as described in the antialiasing section to create a blurred depth of field effect. If objects are close to the depth of field they will have relatively little jitter and will be sharper, while objects far from the depth of field will be blurry and appear out of focus.

Scenes
    - line of suzanne's
    - cows looking at reflective ball
    - 
