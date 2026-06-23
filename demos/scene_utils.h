#pragma once

#include <hiprt/hiprt.h>
#include <vector>
#include <string>

struct Camera {
    hiprtFloat3 origin;
    hiprtFloat3 dir;
};

extern std::vector<hiprtFloat3> g_dynamicVertices;
extern std::vector<uint32_t> g_dynamicIndices;
extern hiprtInstance g_instances[1];

bool loadObjModel(const std::string& filename);
hiprtScene buildScene(hiprtContext context, hiprtGeometry& outGeometry, bool isCpuOnly);
void saveImagePPM(const std::string& filename, uint32_t width, uint32_t height, const std::vector<hiprtHit>& hits, const std::vector<bool>& isGpu);
