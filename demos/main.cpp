#include <hiprt/hiprt.h>
#include <Orochi/Orochi.h>
#include <iostream>
#include "scene_utils.h"
#include "demo_scenarios.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "========================================================\n";
        std::cout << "ZUNIFIKOWANE API HIPRT - DEMO PREZENTACYJNE\n";
        std::cout << "========================================================\n";
        std::cout << "Uzycie: ./showcase [1-6] [opcjonalny_model.obj]\n\n";
        std::cout << "1 - Nieustraszony Fallback (Graceful Degradation)\n";
        std::cout << "2 - AVX Turbo (Porownanie skalar vs AVX8)\n";
        std::cout << "3 - Suwak Mocy (5 klatek z dynamicznym podzialem)\n";
        std::cout << "4 - Szachownica (Zlozony, przeplatany dispatch GPU/CPU)\n";
        std::cout << "5 - Magnum Opus (Dual Dispatch / Managed Memory)\n";
        std::cout << "6 - Stress Test (Suwak Mocy na ogromnej scenie)\n";
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
