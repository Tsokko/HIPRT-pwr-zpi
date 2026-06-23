#include "scene_utils.h"
#include <iostream>
#include <fstream>
#include <Orochi/Orochi.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "../contrib/tinyobjloader/tiny_obj_loader.h"

std::vector<hiprtFloat3> g_dynamicVertices;
std::vector<uint32_t> g_dynamicIndices;
hiprtInstance g_instances[1];

bool loadObjModel(const std::string& filename) {
    tinyobj::ObjReaderConfig reader_config;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filename, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        return false;
    }
    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }
    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    for (size_t s = 0; s < shapes.size(); s++) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            int fv = shapes[s].mesh.num_face_vertices[f];
            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                hiprtFloat3 vertex;
                vertex.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                vertex.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                vertex.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                g_dynamicVertices.push_back(vertex);
                g_dynamicIndices.push_back(g_dynamicIndices.size());
            }
            index_offset += fv;
        }
    }
    return true;
}

hiprtScene buildScene(hiprtContext context, hiprtGeometry& outGeometry, bool isCpuOnly) {
    hiprtTriangleMeshPrimitive mesh = {};
    oroDeviceptr d_vertices = 0;
    oroDeviceptr d_indices = 0;
    oroDeviceptr d_instances = 0;

    if (!isCpuOnly) {
        if (oroMallocManaged((void**)&d_vertices, g_dynamicVertices.size() * sizeof(hiprtFloat3), 1) != 0) {
            std::cerr << "Blad: oroMallocManaged (wierzcholki)!\n";
            exit(-1);
        }
        oroMemcpyHtoD(d_vertices, g_dynamicVertices.data(), g_dynamicVertices.size() * sizeof(hiprtFloat3));

        if (oroMallocManaged((void**)&d_indices, g_dynamicIndices.size() * sizeof(uint32_t), 1) != 0) {
            std::cerr << "Blad: oroMallocManaged (indeksy)!\n";
            exit(-1);
        }
        oroMemcpyHtoD(d_indices, g_dynamicIndices.data(), g_dynamicIndices.size() * sizeof(uint32_t));

        mesh.vertices = (void*)d_vertices;
        mesh.triangleIndices = (void*)d_indices;
    } else {
        mesh.vertices = (void*)g_dynamicVertices.data();
        mesh.triangleIndices = (void*)g_dynamicIndices.data();
    }

    mesh.vertexCount = static_cast<uint32_t>(g_dynamicVertices.size());
    mesh.vertexStride = sizeof(hiprtFloat3);
    mesh.triangleCount = static_cast<uint32_t>(g_dynamicIndices.size() / 3);
    mesh.triangleStride = 3 * sizeof(uint32_t);

    hiprtGeometryBuildInput geomInput = {};
    geomInput.type = hiprtPrimitiveTypeTriangleMesh;
    geomInput.primitive.triangleMesh = mesh;

    hiprtBuildOptions buildOptions = {};
    buildOptions.buildFlags = hiprtBuildFlagBitPreferFastBuild;

    if (hiprtCreateGeometry(context, geomInput, buildOptions, outGeometry) != hiprtSuccess) return nullptr;
    hiprtBuildGeometry(context, hiprtBuildOperationBuild, geomInput, buildOptions, nullptr, nullptr, outGeometry);

    g_instances[0].type = hiprtInstanceTypeGeometry;
    g_instances[0].geometry = outGeometry;

    hiprtSceneBuildInput sceneInput = {};
    sceneInput.instanceCount = 1;
    sceneInput.instanceMasks = nullptr;
    sceneInput.instanceTransformHeaders = nullptr;

    if (!isCpuOnly) {
        if (oroMallocManaged((void**)&d_instances, 1 * sizeof(hiprtInstance), 1) != 0) {
            std::cerr << "Blad: oroMallocManaged (instancje)!\n";
            exit(-1);
        }
        oroMemcpyHtoD(d_instances, g_instances, 1 * sizeof(hiprtInstance));
        sceneInput.instances = (hiprtInstance*)d_instances;
    } else {
        sceneInput.instances = g_instances;
    }

    hiprtScene scene = nullptr;
    if (hiprtCreateScene(context, sceneInput, buildOptions, scene) != hiprtSuccess) return nullptr;
    hiprtBuildScene(context, hiprtBuildOperationBuild, sceneInput, buildOptions, nullptr, nullptr, scene);

    return scene;
}

void saveImagePPM(const std::string& filename, uint32_t width, uint32_t height, const std::vector<hiprtHit>& hits, const std::vector<bool>& isGpu) {
    std::ofstream out(filename);
    out << "P3\n" << width << " " << height << "\n255\n";

    for (size_t i = 0; i < hits.size(); ++i) {
        if (hits[i].hasHit()) {
            if (isGpu[i]) out << "100 255 100 "; // Zielony GPU
            else          out << "100 100 255 "; // Niebieski CPU
        } else {
            out << "30 30 50 "; // Ciemne tlo
        }
        if ((i + 1) % width == 0) out << "\n";
    }
}
