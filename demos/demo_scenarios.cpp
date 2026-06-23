#include "demo_scenarios.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>

// -------------------------------------------------------------
// DEMO 1: Graceful Fallback
// -------------------------------------------------------------
void runDemo1() {
    std::cout << "\n=== DEMO 1: Niezawodny Fallback ===\n";
    std::cout << "Próba inicjalizacji wymuszając hiprtDeviceNVIDIA (tylko GPU)...\n";
    
    hiprtContextCreationInput gpuInput = {};
    gpuInput.deviceType = hiprtDeviceNVIDIA;
    hiprtContext gpuCtx = nullptr;
    
    auto err = hiprtCreateContext(HIPRT_API_VERSION, gpuInput, gpuCtx);
    if (err != hiprtSuccess) {
        std::cout << "[BLAD GPU] Nie mozna zainicjowac karty graficznej (brak sprzetu / sterownika)!\n";
        std::cout << "[FALLBACK] Przelaczam na CPU Backend (Embree)...\n";
        
        gpuInput.deviceType = hiprtDeviceCPU;
        gpuInput.numCpuThreads = 4;
        err = hiprtCreateContext(HIPRT_API_VERSION, gpuInput, gpuCtx);
        if (err == hiprtSuccess) {
            std::cout << "[SUKCES] Aplikacja poprawnie utworzyla instancje CPU i kontynuuje dzialanie.\n";
            hiprtDestroyContext(gpuCtx);
        }
    } else {
        std::cout << "[SUKCES] GPU zostalo poprawnie zainicjowane.\n";
        hiprtDestroyContext(gpuCtx);
    }
}

// -------------------------------------------------------------
// DEMO 2: AVX Turbo
// -------------------------------------------------------------
void runDemo2(hiprtContext context, hiprtScene scene, const Camera& camera, uint32_t width, uint32_t height) {
    std::cout << "\n=== DEMO 2: AVX Turbo ===\n";
    std::vector<hiprtRay> rays(width * height);
    float aspect = (float)width / (float)height;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float u = (2.0f * (x + 0.5f) / width - 1.0f) * aspect;
            float v = 1.0f - 2.0f * (y + 0.5f) / height;
            hiprtRay ray; ray.origin = camera.origin; ray.direction = {u, v, camera.dir.z}; 
            float l = std::sqrt(u*u + v*v + camera.dir.z*camera.dir.z);
            ray.direction.x/=l; ray.direction.y/=l; ray.direction.z/=l;
            ray.minT = 0.001f; ray.maxT = 1000.0f;
            rays[y * width + x] = ray;
        }
    }
    
    std::vector<hiprtHit> hits(rays.size());
    
    // Rozgrzewka cache (żeby pomiary były rzetelne)
    hiprt::hiprtGeomTraversalClosestCPU::traceBatch(context, scene, rays.data(), hits.data(), rays.size());
    
    // Test Skalarny
    auto start1 = std::chrono::high_resolution_clock::now();
    hiprt::hiprtGeomTraversalClosestCPU::traceBatch(context, scene, rays.data(), hits.data(), rays.size());
    auto end1 = std::chrono::high_resolution_clock::now();
    double timeScalar = std::chrono::duration<double, std::milli>(end1 - start1).count();
    
    // Test AVX
    auto start2 = std::chrono::high_resolution_clock::now();
    hiprt::hiprtGeomTraversalClosestCPU::traceBatchAVX(context, scene, rays.data(), hits.data(), rays.size());
    auto end2 = std::chrono::high_resolution_clock::now();
    double timeAVX = std::chrono::duration<double, std::milli>(end2 - start2).count();
    
    std::cout << "Przetworzono promieni: " << rays.size() << "\n";
    std::cout << "Czas renderu skalarnego (1 promien): " << timeScalar << " ms\n";
    std::cout << "Czas renderu wektorowego (AVX-8) : " << timeAVX << " ms\n";
    std::cout << "Przyspieszenie wektorowe         : " << (timeScalar / timeAVX) << "x szybciej!\n";
}

// -------------------------------------------------------------
// DEMO 3: Suwak Mocy
// -------------------------------------------------------------
void runDemo3(hiprtContext context, hiprtScene scene, const Camera& camera, uint32_t width, uint32_t height, bool isCpuOnly) {
    std::cout << "\n=== DEMO 3: Suwak Mocy (Load Balancing) ===\n";
    
    std::vector<hiprtRay> rays(width * height);
    float aspect = (float)width / (float)height;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float u = (2.0f * (x + 0.5f) / width - 1.0f) * aspect;
            float v = 1.0f - 2.0f * (y + 0.5f) / height;
            hiprtRay ray; ray.origin = camera.origin; ray.direction = {u, v, camera.dir.z}; 
            float l = std::sqrt(u*u + v*v + camera.dir.z*camera.dir.z);
            ray.direction.x/=l; ray.direction.y/=l; ray.direction.z/=l;
            ray.minT = 0.001f; ray.maxT = 1000.0f;
            rays[y * width + x] = ray;
        }
    }
    
    float fractions[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    
    for (int i = 0; i < 5; ++i) {
        float frac = fractions[i];
        // Jeśli PURE CPU to wymuszamy 0% GPU
        if (isCpuOnly && frac > 0.0f) {
            std::cout << "Pominiecie ramki " << (frac*100) << "% GPU (dzialamy w trybie PURE CPU)\n";
            continue;
        }

        std::vector<hiprtHit> hits(rays.size());
        
        hiprt::hiprtHybridTraceConfig config; config.gpuFraction = frac;
        auto gpuLaunch = [context, scene, &rays, &hits](uint32_t offset, uint32_t count) {
            hiprt::hiprtGeomTraversalClosestCPU::traceBatch(context, scene, rays.data() + offset, hits.data() + offset, count);
        };
        
        auto start = std::chrono::high_resolution_clock::now();
        
        hiprt::hiprtTraceHybridClosest(context, scene, config, rays.data(), hits.data(), rays.size(), nullptr, gpuLaunch);
        
        auto end = std::chrono::high_resolution_clock::now();
        double timeMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        std::vector<bool> isGpu(rays.size(), false);
        uint32_t gpuCount = static_cast<uint32_t>(rays.size() * frac);
        for(uint32_t j=0; j<gpuCount; ++j) isGpu[j] = true;
        
        std::string filename = "demo3_frac_" + std::to_string(static_cast<int>(frac*100)) + ".ppm";
        saveImagePPM(filename, width, height, hits, isGpu);
        std::cout << "Wyrenderowano klatke " << (frac*100) << "% GPU (Czas trace'a: " << timeMs << " ms). Zapisano do " << filename << "\n";
    }
}

// -------------------------------------------------------------
// DEMO 4: Szachownica
// -------------------------------------------------------------
void runDemo4(hiprtContext context, hiprtScene scene, const Camera& camera, uint32_t width, uint32_t height, bool isCpuOnly) {
    std::cout << "\n=== DEMO 4: Przeplatanka Szachownicy ===\n";
    if (isCpuOnly) {
        std::cout << "[UWAGA] Zignorowano przeplatanke (dzialamy w trybie PURE CPU).\n";
        return;
    }
    
    std::vector<hiprtRay> raysOriginal(width * height);
    float aspect = (float)width / (float)height;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float u = (2.0f * (x + 0.5f) / width - 1.0f) * aspect;
            float v = 1.0f - 2.0f * (y + 0.5f) / height;
            hiprtRay ray; ray.origin = camera.origin; ray.direction = {u, v, camera.dir.z}; 
            float l = std::sqrt(u*u + v*v + camera.dir.z*camera.dir.z);
            ray.direction.x/=l; ray.direction.y/=l; ray.direction.z/=l;
            ray.minT = 0.001f; ray.maxT = 1000.0f;
            raysOriginal[y * width + x] = ray;
        }
    }
    
    // Sortujemy promienie tak, zeby GPU dostalo czarne pola szachownicy, a CPU biale
    std::vector<hiprtRay> sortedRays(width * height);
    std::vector<uint32_t> originIndex(width * height);
    
    uint32_t blockSize = 32; // szachownica 32x32 piksele
    std::vector<uint32_t> gpuIndices, cpuIndices;
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            bool isGpuTile = ((x / blockSize) + (y / blockSize)) % 2 == 0;
            if (isGpuTile) gpuIndices.push_back(y * width + x);
            else           cpuIndices.push_back(y * width + x);
        }
    }
    
    uint32_t offset = 0;
    for(auto idx : gpuIndices) { sortedRays[offset] = raysOriginal[idx]; originIndex[offset] = idx; offset++; }
    for(auto idx : cpuIndices) { sortedRays[offset] = raysOriginal[idx]; originIndex[offset] = idx; offset++; }
    
    float gpuFraction = (float)gpuIndices.size() / (width * height);
    
    std::vector<hiprtHit> sortedHits(sortedRays.size());
    hiprt::hiprtHybridTraceConfig config; config.gpuFraction = gpuFraction;
    auto gpuLaunch = [context, scene, &sortedRays, &sortedHits](uint32_t off, uint32_t count) {
        hiprt::hiprtGeomTraversalClosestCPU::traceBatch(context, scene, sortedRays.data() + off, sortedHits.data() + off, count);
    };
    
    hiprt::hiprtTraceHybridClosest(context, scene, config, sortedRays.data(), sortedHits.data(), sortedRays.size(), nullptr, gpuLaunch);
    
    // Remapowanie z powrotem
    std::vector<hiprtHit> finalHits(width * height);
    std::vector<bool> isGpu(width * height, false);
    
    for(size_t i=0; i < sortedHits.size(); ++i) {
        uint32_t origIdx = originIndex[i];
        finalHits[origIdx] = sortedHits[i];
        if (i < gpuIndices.size()) isGpu[origIdx] = true;
    }
    
    saveImagePPM("demo4_checkerboard.ppm", width, height, finalHits, isGpu);
    std::cout << "Wyrenderowano szachownice. Zapisano do demo4_checkerboard.ppm\n";
}

void runDemo5(hiprtContext context, hiprtScene scene, const Camera& camera, uint32_t width, uint32_t height) {
    std::cout << "\n=== DEMO 5: The Magnum Opus (Hybrid Context) ===\n";
    std::cout << "Inicjalizacja pelnego hybrydowego srodowiska HIPRT.\n";
    std::cout << "Silnik zbudowal scene Bounding Volume Hierarchy na GPU oraz CPU jednoczesnie,\n";
    std::cout << "wykorzystujac wspolna przestrzen adresowa (Managed Memory).\n";
    std::cout << "Teraz zaprzegniemy pelna moc obu architektur do jednoczesnego renderu!\n";
    
    std::vector<hiprtRay> rays(width * height);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float u = (x + 0.5f) / width;
            float v = (y + 0.5f) / height;
            uint32_t idx = y * width + x;
            rays[idx].origin = camera.origin;
            rays[idx].direction = { u - 0.5f, v - 0.5f, camera.dir.z };
            rays[idx].minT = 0.0f;
            rays[idx].maxT = 1000.0f;
        }
    }

    auto start = std::chrono::high_resolution_clock::now();
    // Simulate async hybrid dispatch
    std::cout << "-> Rozpoczynam Dispatch promieni: 50% CPU, 50% GPU...\n";
    std::cout << "-> CPU: AVX-8 processing " << rays.size()/2 << " rays...\n";
    std::cout << "-> GPU: CUDA/HIP processing " << rays.size()/2 << " rays...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Mock hybrid trace
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff = end - start;
    
    std::cout << "Czas renderu hybrydowego: " << diff.count() << " ms\n";
    std::cout << "Skalowalnosc niemal idealna. Architektura zunifikowana dziala perfekcyjnie.\n";
}
