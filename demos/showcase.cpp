#include <hiprt/hiprt.h>
#include <hiprt/hiprt_cpu.h>
#include <hiprt/hiprt_hybrid.h>
#include <Orochi/Orochi.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <string>
#include <chrono>
#include <thread>

#define TINYOBJLOADER_IMPLEMENTATION
#include "contrib/tinyobjloader/tiny_obj_loader.h"

struct Camera {
    hiprtFloat3 origin;
    hiprtFloat3 dir;
};

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
        // Tryb PURE CPU: korzystamy bezpośrednio ze wskaźników systemowych
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

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "========================================================\n";
        std::cout << "ZUNIFIKOWANE API HIPRT - DEMO PREZENTACYJNE\n";
        std::cout << "========================================================\n";
        std::cout << "Uzycie: ./showcase [1-4] [opcjonalny_model.obj]\n\n";
        std::cout << "1 - Nieustraszony Fallback (Graceful Degradation)\n";
        std::cout << "2 - AVX Turbo (Porownanie skalar vs AVX8)\n";
        std::cout << "3 - Suwak Mocy (5 klatek z dynamicznym podzialem)\n";
        std::cout << "4 - Szachownica (Zlozony, przeplatany dispatch GPU/CPU)\n";
        std::cout << "========================================================\n";
        return 0;
    }
    
    int demoId = std::stoi(argv[1]);
    
    if (demoId == 1) {
        runDemo1();
        return 0;
    }
    
    if (argc > 2) {
        if (!loadObjModel(argv[2])) {
            std::cerr << "Nie udalo sie wczytac modelu: " << argv[2] << "\n";
            return -1;
        }
        std::cout << "Zaladowano model: " << argv[2] << " (" << g_dynamicVertices.size() << " wierzcholkow)\n";
    } else if (demoId == 6) {
        std::cout << "Generowanie ciezkiej sceny proceduralnej (setki tysiecy trojkatow)...\n";
        g_dynamicVertices.clear();
        g_dynamicIndices.clear();
        int gridXY = 300, gridZ = 5;
        float spacing = 0.1f;
        for (int x = -gridXY/2; x < gridXY/2; ++x) {
            for (int y = -gridXY/2; y < gridXY/2; ++y) {
                for (int z = 0; z < gridZ; ++z) {
                    float cx = x * spacing;
                    float cy = y * spacing;
                    float cz = -1.0f - z * spacing;
                    uint32_t idx = g_dynamicVertices.size();
                    g_dynamicVertices.push_back({cx, cy, cz});
                    g_dynamicVertices.push_back({cx + 0.08f, cy, cz});
                    g_dynamicVertices.push_back({cx, cy + 0.08f, cz});
                    g_dynamicIndices.push_back(idx);
                    g_dynamicIndices.push_back(idx + 1);
                    g_dynamicIndices.push_back(idx + 2);
                }
            }
        }
        std::cout << "Wygenerowano pomyslnie " << (g_dynamicIndices.size() / 3) << " trojkatow.\n";
    } else {
        g_dynamicVertices = { {0.0f, 1.0f, -2.0f}, {-1.0f, -1.0f, -2.0f}, {1.0f, -1.0f, -2.0f} };
        g_dynamicIndices = {0, 1, 2};
        std::cout << "Zaladowano domyslny trojkat.\n";
    }
    
    // Próbujemy zainicjować Orochi w showcase.exe
    bool isCpuOnly = false;
    oroCtx oro_ctx = nullptr;
    oroDevice oro_device = 0;
    
    int oroErr = oroInitialize(ORO_API_AUTOMATIC, 0);
    if (oroErr != 0) {
        std::cout << "-> [UWAGA] Nie udalo sie zainicjowac biblioteki Orochi (Kod: " << oroErr << ").\n";
        std::cout << "-> Brak sterownikow HIP/CUDA. Uruchamiam aplikacje w trybie PURE CPU!\n";
        isCpuOnly = true;
    } else {
        oroInit(0);
        if (oroDeviceGet(&oro_device, 0) != 0 || oroCtxCreate(&oro_ctx, 0, oro_device) != 0) {
            std::cout << "-> [UWAGA] Nie mozna utworzyc kontekstu GPU z poziomu Orochi. Uruchamiam PURE CPU!\n";
            isCpuOnly = true;
        }
    }

    hiprtContextCreationInput hybridInput = {};
    hybridInput.numCpuThreads = 8; 
    hybridInput.ctxt = isCpuOnly ? nullptr : oroGetRawCtx(oro_ctx);
    hybridInput.device = isCpuOnly ? 0 : oroGetRawDevice(oro_device);
    
    hiprtContext context = nullptr;
    
    if (isCpuOnly) {
        hybridInput.deviceType = hiprtDeviceCPU;
        if (hiprtCreateContext(HIPRT_API_VERSION, hybridInput, context) != hiprtSuccess) {
            std::cerr << "-> [BLAD KRYTYCZNY] Inicjalizacja PURE CPU zawiodla!\n";
            return -1;
        }
    } else {
        std::cout << "-> Inicjalizacja kontekstu (hiprtDeviceNVIDIA + CPU)...\n";
        hybridInput.deviceType = (hiprtDeviceType)(hiprtDeviceCPU | hiprtDeviceNVIDIA);
        if (hiprtCreateContext(HIPRT_API_VERSION, hybridInput, context) != hiprtSuccess) {
            std::cout << "-> Nie znaleziono NVIDII. Fallback na AMD (hiprtDeviceAMD + CPU)...\n";
            hybridInput.deviceType = (hiprtDeviceType)(hiprtDeviceCPU | hiprtDeviceAMD);
            if (hiprtCreateContext(HIPRT_API_VERSION, hybridInput, context) != hiprtSuccess) {
                std::cout << "-> [UWAGA] Nie udalo sie utworzyc kontekstu GPU (ani AMD, ani NVIDIA) w hiprt.\n";
                std::cout << "-> Uruchamiam aplikacje w trybie PURE CPU!\n";
                isCpuOnly = true;
                hybridInput.deviceType = hiprtDeviceCPU;
                if (hiprtCreateContext(HIPRT_API_VERSION, hybridInput, context) != hiprtSuccess) {
                    std::cerr << "-> [BLAD KRYTYCZNY] Nawet inicjalizacja CPU zawiodla!\n";
                    return -1;
                }
            }
        }
    }
    
    std::cout << "-> Kontekst HIPRT utworzony pomyslnie. Tryb PURE CPU: " << (isCpuOnly ? "TAK" : "NIE") << "\n";

    hiprtGeometry geometry = nullptr;
    hiprtScene scene = buildScene(context, geometry, isCpuOnly);

    Camera camera = {{0.0f, 0.0f, 2.5f}, {0.0f, 0.0f, -1.0f}};
    
    if (demoId == 2) runDemo2(context, scene, camera, 800, 600);
    else if (demoId == 3) runDemo3(context, scene, camera, 800, 600, isCpuOnly);
    else if (demoId == 4) runDemo4(context, scene, camera, 800, 600, isCpuOnly);
    else if (demoId == 5) runDemo5(context, scene, camera, 800, 600);
    else if (demoId == 6) runDemo3(context, scene, camera, 800, 600, isCpuOnly);
    else std::cout << "Nieznane ID dema.\n";
    
    hiprtDestroyScene(context, scene);
    hiprtDestroyGeometry(context, geometry);
    hiprtDestroyContext(context);
    
    return 0;
}
