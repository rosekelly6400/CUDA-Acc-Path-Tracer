#include "scene.h"

#include "utilities.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>
#include "json.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "tiny_gltf_v3.h"

using namespace std;
using json = nlohmann::json;

static int cmp_str_int_pair(const void* a, const void* b) {
    const tg3_str_int_pair* pa = (const tg3_str_int_pair*)a;
    const tg3_str_int_pair* pb = (const tg3_str_int_pair*)b;
    uint32_t la = pa->key.len, lb = pb->key.len;
    uint32_t m = la < lb ? la : lb;
    int r = memcmp(pa->key.data, pb->key.data, m);
    if (r) return r;
    return (la < lb) ? -1 : (la > lb ? 1 : 0);
}

static void d_attrs(const tg3_str_int_pair* attrs, uint32_t n) {
    tg3_str_int_pair* sorted;
    uint32_t i;
    if (n == 0) { fputs("[]", stdout); return; }
    sorted = (tg3_str_int_pair*)malloc(n * sizeof(*sorted));
    memcpy(sorted, attrs, n * sizeof(*sorted));
    qsort(sorted, n, sizeof(*sorted), cmp_str_int_pair);
    putchar('[');
    for (i = 0; i < n; ++i) {
        if (i) putchar(',');
        printf("%.*s:%d", (int)sorted[i].key.len, sorted[i].key.data, sorted[i].value);
    }
    putchar(']');
    free(sorted);
}

Scene::Scene(string filename)
{
    cout << "Reading scene from " << filename << " ..." << endl;
    cout << " " << endl;
    auto ext = filename.substr(filename.find_last_of('.'));
    if (ext == ".json")
    {
        loadFromJSON(filename);
        return;
    }
    else
    {
        cout << "Couldn't read from " << filename << endl;
        exit(-1);
    }
}

glm::vec3 multiplyMV2(glm::mat4 m, glm::vec4 v)
{
    return glm::vec3(m * v);
    //return glm::vec3(0.0f);
}

Bounds createBoundsFromVerts(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, Geom& triGeom) {
    Bounds newBounds;
    glm::vec3 minCorner;
    glm::vec3 maxCorner;

    v0 = multiplyMV2(triGeom.transform, glm::vec4(v0, 1.0f));
    v1 = multiplyMV2(triGeom.transform, glm::vec4(v1, 1.0f));
    v2 = multiplyMV2(triGeom.transform, glm::vec4(v2, 1.0f));

    for (int i = 0; i < 3; i++)
    {
        minCorner[i] = std::min({ v0[i], v1[i], v2[i] });
        maxCorner[i] = std::max({ v0[i], v1[i], v2[i] });
    }

    newBounds.minCorner = minCorner;
    newBounds.maxCorner = maxCorner;
    return newBounds;
}

Bounds createBoundsFromBounds(Bounds b0, Bounds b1) {
    Bounds newBounds;
    glm::vec3 minCorner;
    glm::vec3 maxCorner;


    for (int i = 0; i < 3; i++)
    {
        minCorner[i] = std::min({ b0.minCorner[i], b1.minCorner[i] });
        maxCorner[i] = std::max({ b0.maxCorner[i], b1.maxCorner[i] });
    }

    newBounds.minCorner = minCorner;
    newBounds.maxCorner = maxCorner;
    return newBounds;
}

Bounds addCentroidToBounds(Bounds b, glm::vec3 centroid) {
    Bounds newBounds;
    glm::vec3 minCorner;
    glm::vec3 maxCorner;


    for (int i = 0; i < 3; i++)
    {
        minCorner[i] = std::min({ b.minCorner[i], centroid[i] });
        maxCorner[i] = std::max({ b.maxCorner[i], centroid[i] });
    }

    newBounds.minCorner = minCorner;
    newBounds.maxCorner = maxCorner;
    return newBounds;
}

int findMaxDim(Bounds b) {
    glm::vec3 diagonal = b.maxCorner - b.minCorner;
    if (diagonal.x > diagonal.y && diagonal.x > diagonal.z) {
        return 0;
    }
    else if (diagonal.y > diagonal.z) {
        return 1;
    }
    else {
        return 2;
    }
}

Geom createBoundingBoxGeomFromBounds(Bounds bounds)
{
    Geom newGeom;
    newGeom.type = CUBE;
    newGeom.materialid = 0;
    const auto& trans = ((bounds.maxCorner + bounds.minCorner) / 2.0f);
    const auto& rotat = glm::vec3(0.0f);
    const auto& scale = glm::vec3(  glm::abs(bounds.maxCorner.x - bounds.minCorner.x),
                                    glm::abs(bounds.maxCorner.y - bounds.minCorner.y),
                                    glm::abs(bounds.maxCorner.z - bounds.minCorner.z));
    newGeom.translation = glm::vec3(trans[0], trans[1], trans[2]);
    newGeom.rotation = glm::vec3(rotat[0], rotat[1], rotat[2]);
    newGeom.scale = glm::vec3(scale[0], scale[1], scale[2]);
    newGeom.transform = utilityCore::buildTransformationMatrix(
        newGeom.translation, newGeom.rotation, newGeom.scale);
    newGeom.inverseTransform = glm::inverse(newGeom.transform);
    newGeom.invTranspose = glm::inverseTranspose(newGeom.transform);
    newGeom.v0 = glm::vec3(0.0f);
    newGeom.v1 = glm::vec3(0.0f);
    newGeom.v2 = glm::vec3(0.0f);

    return newGeom;
}

void printNode(BVHNode node) {
    fprintf(stderr, "num prims: [%d] \n", node.numPrims);
    fprintf(stderr, "first prim offset: [%d] \n", node.firstPrimOffset);
    fprintf(stderr, "first child index: [%d] \n", node.bvhNodeChildIndex_First);
    fprintf(stderr, "second child index: [%d] \n", node.bvhNodeChildIndex_Second);
}

void printPrim(BVHPrimitive prim) {
    fprintf(stderr, "leaf geom index: [%d] \n", prim.leafGeomIndex);
}

// make empty bvh node and pass it in, this function populates it and creates its children then passes them into recursive calls
void recursiveBVHBuild(BVHNode& bvhNode, BVHNode* bvhNodes, std::vector<BVHPrimitive>& bvhPrimitives, std::vector<BVHPrimitive>& orderedPrims, int start, int end, int& numNodes)
{
    // calculate bounds for all primitives from start to end
    bvhNode.boundingCorners = bvhPrimitives[0].boundingCorners;
    for (int i = start; i < end; i++) {
        BVHPrimitive p = bvhPrimitives[i];
        bvhNode.boundingCorners = createBoundsFromBounds(bvhNode.boundingCorners, p.boundingCorners);
    }
    bvhNode.boundingBox = createBoundingBoxGeomFromBounds(bvhNode.boundingCorners);

    int numPrimitives = end - start;
    // if only one primitive, create leaf
    if (numPrimitives == 1) {
        bvhNode.firstPrimOffset = orderedPrims.size();
        for (int i = start; i < end; ++i) {
            orderedPrims.push_back(bvhPrimitives[i]);
        }
        bvhNode.numPrims = numPrimitives;
        return;
    }
    else {
        // calculate centroid bounds for primitives from start to end
        Bounds centroidBounds;
        centroidBounds.minCorner = (bvhPrimitives[start].boundingCorners.minCorner + bvhPrimitives[start].boundingCorners.maxCorner) / 2.0f;
        centroidBounds.maxCorner = (bvhPrimitives[start].boundingCorners.minCorner + bvhPrimitives[start].boundingCorners.maxCorner) / 2.0f;
        for (int i = start; i < end; i++) {
            BVHPrimitive p = bvhPrimitives[i];
            glm::vec3 p_centroid = (p.boundingCorners.minCorner + p.boundingCorners.maxCorner) / 2.0f;
            centroidBounds = addCentroidToBounds(centroidBounds, p_centroid);
        }
        int maxDim = findMaxDim(centroidBounds);

        int mid = (start + end) / 2;
        if (centroidBounds.maxCorner[maxDim] == centroidBounds.minCorner[maxDim]) {
            bvhNode.firstPrimOffset = orderedPrims.size();
            for (int i = start; i < end; ++i) {
                orderedPrims.push_back(bvhPrimitives[i]);
            }
            bvhNode.numPrims = numPrimitives;
            return;
        }
        else {
            // partition through node's centroids
            float pmid = (centroidBounds.maxCorner[maxDim] + centroidBounds.minCorner[maxDim]) / 2.0f;
            auto sortedMid = std::partition(bvhPrimitives.begin() + start, bvhPrimitives.begin() + (end - 1) + 1, [maxDim, pmid](const BVHPrimitive prim) {
                float primCentroidDim = ((prim.boundingCorners.maxCorner + prim.boundingCorners.minCorner) / 2.0f)[maxDim];
                return primCentroidDim < pmid;
                });
            mid = sortedMid - bvhPrimitives.begin();

            //recursively create subnodes
            bvhNode.dim = maxDim;
            BVHNode child1;
            BVHNode child2;
            bvhNodes[numNodes] = child1;
            bvhNode.bvhNodeChildIndex_First = numNodes;
            numNodes++;
            bvhNodes[numNodes] = child2;
            bvhNode.bvhNodeChildIndex_Second = numNodes;
            numNodes++;
            recursiveBVHBuild(bvhNodes[bvhNode.bvhNodeChildIndex_First], bvhNodes, bvhPrimitives, orderedPrims, start, mid, numNodes);
            recursiveBVHBuild(bvhNodes[bvhNode.bvhNodeChildIndex_Second], bvhNodes, bvhPrimitives, orderedPrims, mid, end, numNodes);

            bvhNode.boundingCorners = createBoundsFromBounds(bvhNodes[bvhNode.bvhNodeChildIndex_First].boundingCorners, bvhNodes[bvhNode.bvhNodeChildIndex_Second].boundingCorners);
            bvhNode.boundingBox = createBoundingBoxGeomFromBounds(bvhNode.boundingCorners);
        }
    }
    return;
}

void Scene::loadFromJSON(const std::string& jsonName)
{
    bool buildBVHTree = false;
    std::ifstream f(jsonName);
    json data = json::parse(f);
    const auto& materialsData = data["Materials"];
    std::unordered_map<std::string, uint32_t> MatNameToID;
    for (const auto& item : materialsData.items())
    {
        const auto& name = item.key();
        const auto& p = item.value();
        Material newMaterial{};
        // TODO: handle materials loading differently
        newMaterial.hasReflective = 0.0f;
        if (p["TYPE"] == "Diffuse")
        {
            const auto& col = p["RGB"];
            newMaterial.color = glm::vec3(col[0], col[1], col[2]);
        }
        else if (p["TYPE"] == "Emitting")
        {
            const auto& col = p["RGB"];
            newMaterial.color = glm::vec3(col[0], col[1], col[2]);
            newMaterial.emittance = p["EMITTANCE"];
        }
        else if (p["TYPE"] == "Specular")
        {
            const auto& col = p["RGB"];
            newMaterial.color = glm::vec3(col[0], col[1], col[2]);
            newMaterial.hasReflective = p["ROUGHNESS"];;
        }
        MatNameToID[name] = materials.size();
        materials.emplace_back(newMaterial);
    }
    const auto& objectsData = data["Objects"];
    for (const auto& p : objectsData)
    {
        const auto& type = p["TYPE"];
        if(type == "modelFile") {
            buildBVHTree = true;

            // add each triangle as a geom here
            std::string relativeFileName = p["FILEPATH"];
            tg3_parse_options opts;
            tg3_error_stack errors;
            tg3_model model;

            tg3_parse_options_init(&opts);
            tg3_error_stack_init(&errors);

            int filenameLength = relativeFileName.length();
            tg3_error_code err = tg3_parse_file(&model, &errors, relativeFileName.c_str(), filenameLength, &opts);
            if (err != TG3_OK) {
                for (uint32_t i = 0; i < errors.count; i++) {
                    fprintf(stderr, "[%d] %s\n", (int)errors.entries[i].severity,
                            errors.entries[i].message ? errors.entries[i].message : "(null)");
                }
            }
            // ... use model ...
            fprintf(stderr, "mesh count: [%d] \n", model.meshes_count);
            for(int mesh_i = 0; mesh_i < model.meshes_count; ++mesh_i)
            {
                const tg3_mesh mesh = model.meshes[mesh_i];
                fprintf(stderr, "prim count: [%d] \n", mesh.primitives_count);
                for(int prim_i = 0; prim_i < mesh.primitives_count; ++prim_i)
                {
                    const tg3_primitive primitive = mesh.primitives[prim_i];
                    fprintf(stderr, "prim mode: [%d] \n", primitive.mode);
                    fprintf(stderr, "prim attr count: [%d] \n", primitive.attributes_count);
                    if(primitive.mode == TG3_MODE_TRIANGLES)
                    {
                        for (int attr_i = 0; attr_i < primitive.attributes_count; ++attr_i) {
                            //fprintf(stderr, "prim attr key: %s \n", primitive.attributes[attr_i].key.data);
                            std::string posStr = "POSITION";
                            if (primitive.attributes[attr_i].key.data == posStr)
                            {
                                fprintf(stderr, "POS Attr value: [%d] \n", primitive.attributes[attr_i].value);
                                const tg3_accessor& accessor = model.accessors[primitive.attributes[attr_i].value];
                                const tg3_buffer_view& bufferView = model.buffer_views[accessor.buffer_view];
                                const tg3_buffer& buffer = model.buffers[bufferView.buffer];
                                const float* positions = reinterpret_cast<const float*>(&buffer.data.data[bufferView.byte_offset + accessor.byte_offset]);
                                fprintf(stderr, "POS accessor count : [%d] \n", accessor.count);
                                int num_tris = accessor.count / 3;
                                // each vertex
                                for (int tri_i = 0; tri_i < num_tris; ++tri_i) {
                                    int pos_tri_start_idx = tri_i * 9;

                                    Geom newGeom;
                                    newGeom.type = TRIANGLE;
                                    newGeom.materialid = MatNameToID[p["MATERIAL"]];
                                    const auto& trans = p["TRANS"];
                                    const auto& rotat = p["ROTAT"];
                                    const auto& scale = p["SCALE"];
                                    newGeom.translation = glm::vec3(trans[0], trans[1], trans[2]);
                                    newGeom.rotation = glm::vec3(rotat[0], rotat[1], rotat[2]);
                                    newGeom.scale = glm::vec3(scale[0], scale[1], scale[2]);
                                    newGeom.transform = utilityCore::buildTransformationMatrix(
                                        newGeom.translation, newGeom.rotation, newGeom.scale);
                                    newGeom.inverseTransform = glm::inverse(newGeom.transform);
                                    newGeom.invTranspose = glm::inverseTranspose(newGeom.transform);
                                    newGeom.v0 = glm::vec3(positions[pos_tri_start_idx + 0 * 3 + 0], positions[pos_tri_start_idx + 0 * 3 + 1], positions[pos_tri_start_idx + 0 * 3 + 2]);
                                    newGeom.v1 = glm::vec3(positions[pos_tri_start_idx + 1 * 3 + 0], positions[pos_tri_start_idx + 1 * 3 + 1], positions[pos_tri_start_idx + 1 * 3 + 2]);
                                    newGeom.v2 = glm::vec3(positions[pos_tri_start_idx + 2 * 3 + 0], positions[pos_tri_start_idx + 2 * 3 + 1], positions[pos_tri_start_idx + 2 * 3 + 2]);

                                    geoms.push_back(newGeom);
                                }
                            }
                        }

                        d_attrs(primitive.attributes, primitive.attributes_count);
                        
                    }
                }
            }
            tg3_model_free(&model);
            tg3_error_stack_free(&errors);
        }
        else {
            Geom newGeom;
            if (type == "cube")
            {
                newGeom.type = CUBE;
            }
            else
            {
                newGeom.type = SPHERE;
            }
            newGeom.materialid = MatNameToID[p["MATERIAL"]];
            const auto& trans = p["TRANS"];
            const auto& rotat = p["ROTAT"];
            const auto& scale = p["SCALE"];
            newGeom.translation = glm::vec3(trans[0], trans[1], trans[2]);
            newGeom.rotation = glm::vec3(rotat[0], rotat[1], rotat[2]);
            newGeom.scale = glm::vec3(scale[0], scale[1], scale[2]);
            newGeom.transform = utilityCore::buildTransformationMatrix(
                newGeom.translation, newGeom.rotation, newGeom.scale);
            newGeom.inverseTransform = glm::inverse(newGeom.transform);
            newGeom.invTranspose = glm::inverseTranspose(newGeom.transform);
            newGeom.v0 = glm::vec3(0.0f);
            newGeom.v1 = glm::vec3(0.0f);
            newGeom.v2 = glm::vec3(0.0f);

            geoms.push_back(newGeom);

            if (materials[newGeom.materialid].emittance > 0.0f ) {
                lights.push_back(newGeom);
            }
        }
    }
    const auto& cameraData = data["Camera"];
    Camera& camera = state.camera;
    RenderState& state = this->state;
    camera.resolution.x = cameraData["RES"][0];
    camera.resolution.y = cameraData["RES"][1];
    float fovy = cameraData["FOVY"];
    state.iterations = cameraData["ITERATIONS"];
    state.traceDepth = cameraData["DEPTH"];
    state.imageName = cameraData["FILE"];
    const auto& pos = cameraData["EYE"];
    const auto& lookat = cameraData["LOOKAT"];
    const auto& up = cameraData["UP"];
    camera.position = glm::vec3(pos[0], pos[1], pos[2]);
    camera.lookAt = glm::vec3(lookat[0], lookat[1], lookat[2]);
    camera.up = glm::vec3(up[0], up[1], up[2]);

    //calculate fov based on resolution
    float yscaled = tan(fovy * (PI / 180));
    float xscaled = (yscaled * camera.resolution.x) / camera.resolution.y;
    float fovx = (atan(xscaled) * 180) / PI;
    camera.fov = glm::vec2(fovx, fovy);

    camera.right = glm::normalize(glm::cross(camera.view, camera.up));
    camera.pixelLength = glm::vec2(2 * xscaled / (float)camera.resolution.x,
        2 * yscaled / (float)camera.resolution.y);

    camera.view = glm::normalize(camera.lookAt - camera.position);

    //set up render camera stuff
    int arraylen = camera.resolution.x * camera.resolution.y;
    state.image.resize(arraylen);
    std::fill(state.image.begin(), state.image.end(), glm::vec3());

    // create BVH Tree

    // create list of prims with bounding boxes
    for (int i = 0; i < geoms.size(); i++) {
        Geom g = geoms[i];
        if (g.type == TRIANGLE) {
            BVHPrimitive newPrim;
            newPrim.boundingCorners = createBoundsFromVerts(g.v0, g.v1, g.v2, g);
            newPrim.leafGeomIndex = i;
            newPrim.boundingBox = createBoundingBoxGeomFromBounds(newPrim.boundingCorners);
            bvhPrimitives.push_back(newPrim);
        }
    }

    if (bvhPrimitives.size() > 0) {
        // create master bounding box
        /*BVHNode bvhNode;
        bvhNode.boundingCorners = bvhPrimitives[0].boundingCorners;

        for (int i = 0; i < bvhPrimitives.size(); i++) {
            BVHPrimitive p = bvhPrimitives[i];
            bvhNode.boundingCorners = createBoundsFromBounds(bvhNode.boundingCorners, p.boundingCorners);
        }
        bvhNode.boundingBox = createBoundingBoxGeomFromBounds(bvhNode.boundingCorners);
        bvhNodes.push_back(bvhNode);*/

        /*BVHNode node;
        recursiveBVHBuild(node, bvhNodes, bvhPrimitives, orderedPrims, 0, 1);*/
        //bvhNodes.push_back(node);

        BVHNode* bvhNodeList = new BVHNode[2 * bvhPrimitives.size()];
        BVHNode node;
        bvhNodeList[0] = node;
        int numNodes = 1;
        recursiveBVHBuild(bvhNodeList[0], bvhNodeList, bvhPrimitives, orderedPrims, 0, bvhPrimitives.size(), numNodes);
        fprintf(stderr, "\n num prims: [%d] \n", bvhPrimitives.size());
        fprintf(stderr, "\n num nodes: [%d] \n", numNodes);
        // put bvhNodeList into vector and then free it
        for (int i = 0; i < numNodes; i++) {
            bvhNodes.push_back(bvhNodeList[i]);
        }

        /*for (int i = 0; i < bvhNodes.size(); i++) {
            fprintf(stderr, "\n");
            printNode(bvhNodes[i]);
        }

        for (int i = 0; i < orderedPrims.size(); i++) {
            printPrim(orderedPrims[i]);
        }

        for (int i = 0; i < geoms.size(); i++) {
            fprintf(stderr, "index: [%d]  type: %d \n", i, geoms[i].type);
        }*/

        delete[] bvhNodeList;
    }
}
