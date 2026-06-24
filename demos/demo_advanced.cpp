#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <chrono>
#include <thread>

#include "demo_scenarios.h"
#include <hiprt/hiprt_cpu.h>
#include <hiprt/hiprt_hybrid.h>

struct Float3 { float x, y, z; };
struct Color { unsigned char r, g, b, a; };

const int WIDTH = 800;
const int HEIGHT = 600;

void RenderFrame(std::vector<Color>& framebuffer, hiprtScene scene /*, const HybridTraceConfig& config */) {
    Float3 lightDir = {0.577f, 0.577f, -0.577f};

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            
            hiprtRay ray;
            ray.origin = {0.0f, 1.0f, -5.0f}; 
            
            float dirX = (x - WIDTH / 2.0f) / WIDTH;
            float dirY = (HEIGHT / 2.0f - y) / HEIGHT;
            float dirZ = 1.0f;
            float length = std::sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
            
            ray.direction = {dirX / length, dirY / length, dirZ / length};
            ray.maxT = 1000.0f;

            // 2. Wywołanie Waszego hybrydowego śledzenia
            // hiprtHit hit = hiprtTraceHybridClosest(scene, ray, config);
            
            // UWAGA: Używam tu pseudokodu z wynikiem 'hit', 
            // dostosuj to do Waszej zwracanej struktury
            bool hasHit = false; /* hit.hasHit; */ 
            int pixelIndex = y * WIDTH + x;
            
            if (hasHit) {
                // 3. Promień cienia (Shadow Ray)
                hiprtRay shadowRay;
                /*
                shadowRay.origin = {
                    hit.position.x + hit.normal.x * 0.001f,
                    hit.position.y + hit.normal.y * 0.001f,
                    hit.position.z + hit.normal.z * 0.001f
                };
                */
                shadowRay.direction = {lightDir.x, lightDir.y, lightDir.z};
                shadowRay.maxT = 100.0f;

                // hiprtHit shadowHit = hiprtTraceHybridClosest(scene, shadowRay, config);
                bool inShadow = false; /* shadowHit.hasHit; */

                if (inShadow) {
                    framebuffer[pixelIndex] = { 40, 40, 40, 255 }; // Cień
                } else {
                    float dotLight = 0.8f; // Placeholder dla Lamberta
                    unsigned char intensity = std::max(40.0f, 255.0f * dotLight);
                    framebuffer[pixelIndex] = { intensity, intensity, intensity, 255 }; // Oświetlone
                }
            } else {
                // Tło: Zrobimy prosty, widoczny gradient do testów
                unsigned char r = (unsigned char)(255.0f * x / WIDTH);
                unsigned char g = (unsigned char)(255.0f * y / HEIGHT);
                unsigned char b = 100;
                framebuffer[pixelIndex] = { r, g, b, 255 }; 
            }
        }
    }
}

void SavePPM(const char* filename, const std::vector<Color>& framebuffer, int width, int height) {
    std::ofstream ofs(filename, std::ios::binary);
    ofs << "P6\n" << width << " " << height << "\n255\n";
    for (int i = 0; i < width * height; ++i) {
        ofs << framebuffer[i].r << framebuffer[i].g << framebuffer[i].b;
    }
    ofs.close();
    std::cout << "-> Zapisano klatkę do pliku: " << filename << std::endl;
}

int main(int argc, char* argv[]) {
    std::vector<Color> framebuffer(WIDTH * HEIGHT);


    hiprtScene dummyScene = nullptr; 

    std::cout << "Rozpoczynam renderowanie klatki..." << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();

    RenderFrame(framebuffer, dummyScene /*, config*/);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << "Czas renderowania: " << diff.count() << " s" << std::endl;

    SavePPM("wyrenderowana_scena.ppm", framebuffer, WIDTH, HEIGHT);

    return 0;
}