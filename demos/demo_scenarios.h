#pragma once

#include <hiprt/hiprt.h>
#include "scene_utils.h"

void runDemo1();
void runDemo2(hiprtContext context, hiprtScene scene, const Camera& camera, uint32_t width, uint32_t height);
void runDemo3(hiprtContext context, hiprtScene scene, const Camera& camera, uint32_t width, uint32_t height, bool isCpuOnly);
void runDemo4(hiprtContext context, hiprtScene scene, const Camera& camera, uint32_t width, uint32_t height, bool isCpuOnly);
void runDemo5(hiprtContext context, hiprtScene scene, const Camera& camera, uint32_t width, uint32_t height);
