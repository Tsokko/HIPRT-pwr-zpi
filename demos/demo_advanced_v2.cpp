#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <algorithm>

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() {}
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3 operator*(float s)       const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s)       const { float i = 1.0f / s; return {x * i, y * i, z * i}; }
    Vec3 operator-()              const { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator*=(const Vec3& o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
    Vec3& operator*=(float s)       { x *= s; y *= s; z *= s; return *this; }
    float operator[](int i)  const { return (&x)[i]; }
    float& operator[](int i)       { return (&x)[i]; }
};
static inline float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float len2(const Vec3& v) { return dot(v, v); }
static inline float len(const Vec3& v)  { return std::sqrt(len2(v)); }
static inline Vec3  norm(const Vec3& v) { return v * (1.0f / len(v)); }
static inline Vec3  cross(const Vec3& a, const Vec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static inline Vec3 reflect(const Vec3& v, const Vec3& n) { return v - n * (2.0f * dot(v, n)); }
static inline Vec3 refract(const Vec3& uv, const Vec3& n, float etai_over_etat) {
    float cos_theta = std::min(dot(-uv, n), 1.0f);
    Vec3 perp = (uv + n * cos_theta) * etai_over_etat;
    Vec3 para = n * -std::sqrt(std::fabs(1.0f - len2(perp)));
    return perp + para;
}
static inline float schlick(float cosine, float ref_idx) {
    float r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
    r0 *= r0;
    return r0 + (1.0f - r0) * std::pow(1.0f - cosine, 5.0f);
}

struct RNG {
    uint64_t state, inc;
    RNG(uint64_t seed = 0, uint64_t seq = 1) {
        state = 0; inc = (seq << 1u) | 1u;
        next_u32(); state += seed; next_u32();
    }
    uint32_t next_u32() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = (uint32_t)(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }
    // [0,1)
    float f() { return (next_u32() >> 8) * (1.0f / 16777216.0f); }
    float f(float lo, float hi) { return lo + (hi - lo) * f(); }
};

static inline Vec3 random_in_unit_disk(RNG& r) {
    for (;;) { Vec3 p{r.f(-1,1), r.f(-1,1), 0}; if (len2(p) < 1.0f) return p; }
}
static inline Vec3 random_in_unit_sphere(RNG& r) {
    for (;;) { Vec3 p{r.f(-1,1), r.f(-1,1), r.f(-1,1)}; if (len2(p) < 1.0f) return p; }
}
static inline Vec3 cosine_sample_hemisphere(const Vec3& n, RNG& rng) {
    float s = std::copysign(1.0f, n.z);
    float a = -1.0f / (s + n.z);
    float b = n.x * n.y * a;
    Vec3 t1{1.0f + s * n.x * n.x * a, s * b, -s * n.x};
    Vec3 t2{b, s + n.y * n.y * a, -n.y};
    float r1 = rng.f(), r2 = rng.f();
    float phi = 6.2831853f * r1;
    float r = std::sqrt(r2);
    float lx = std::cos(phi) * r, ly = std::sin(phi) * r, lz = std::sqrt(1.0f - r2);
    return t1 * lx + t2 * ly + n * lz;
}

enum MatType { DIFFUSE, METAL, GLASS, LIGHT };
struct Sphere {
    Vec3 center; float radius;
    int  mat; Vec3 albedo; float param; 
};

struct Hit {
    float t; Vec3 p, n; bool front;
    int mat; Vec3 albedo; float param;
};
static inline bool sphere_hit(const Sphere& s, const Vec3& o, const Vec3& d,
                              float tmin, float tmax, Hit& rec) {
    Vec3 oc = o - s.center;
    float a = len2(d), half_b = dot(oc, d), c = len2(oc) - s.radius * s.radius;
    float disc = half_b * half_b - a * c;
    if (disc < 0) return false;
    float sq = std::sqrt(disc);
    float root = (-half_b - sq) / a;
    if (root < tmin || root > tmax) {
        root = (-half_b + sq) / a;
        if (root < tmin || root > tmax) return false;
    }
    rec.t = root; rec.p = o + d * root;
    Vec3 outward = (rec.p - s.center) / s.radius;
    rec.front = dot(d, outward) < 0;
    rec.n = rec.front ? outward : -outward;
    rec.mat = s.mat; rec.albedo = s.albedo; rec.param = s.param;
    return true;
}

struct AABB {
    Vec3 mn{ 1e30f, 1e30f, 1e30f}, mx{-1e30f,-1e30f,-1e30f};
    void grow(const Vec3& p) {
        mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
    }
    void grow(const AABB& b) { grow(b.mn); grow(b.mx); }
    Vec3 centroid() const { return (mn + mx) * 0.5f; }
};
static inline AABB sphere_box(const Sphere& s) {
    Vec3 r{s.radius, s.radius, s.radius};
    AABB b; b.mn = s.center - r; b.mx = s.center + r; return b;
}
struct BVHNode { AABB box; int left, right, first, count; }; // count>0 => liść

struct BVH {
    std::vector<Sphere> spheres;
    std::vector<int>    idx;     
    std::vector<BVHNode> nodes;

    void build(std::vector<Sphere> s) {
        spheres = std::move(s);
        idx.resize(spheres.size());
        for (int i = 0; i < (int)idx.size(); ++i) idx[i] = i;
        nodes.reserve(spheres.size() * 2);
        build_range(0, (int)idx.size());
    }
    int build_range(int start, int end) {
        int node_id = (int)nodes.size();
        nodes.push_back({});
        AABB box;
        for (int i = start; i < end; ++i) box.grow(sphere_box(spheres[idx[i]]));
        int n = end - start;
        if (n <= 2) {                                 
            nodes[node_id] = {box, -1, -1, start, n};
            return node_id;
        }
        Vec3 ext = box.mx - box.mn;                    
        int axis = (ext.x > ext.y) ? (ext.x > ext.z ? 0 : 2) : (ext.y > ext.z ? 1 : 2);
        int mid = start + n / 2;
        std::nth_element(idx.begin() + start, idx.begin() + mid, idx.begin() + end,
            [&](int a, int b) {
                return sphere_box(spheres[a]).centroid()[axis]
                     < sphere_box(spheres[b]).centroid()[axis];
            });
        int l = build_range(start, mid);
        int r = build_range(mid, end);
        nodes[node_id] = {box, l, r, 0, 0};
        return node_id;
    }
    bool hit(const Vec3& o, const Vec3& d, Hit& best) const {
        Vec3 invD{1.0f/d.x, 1.0f/d.y, 1.0f/d.z};
        float closest = 1e30f; bool found = false;
        int stack[64]; int sp = 0; stack[sp++] = 0;
        while (sp) {
            const BVHNode& nd = nodes[stack[--sp]];
            float t0x = (nd.box.mn.x - o.x) * invD.x, t1x = (nd.box.mx.x - o.x) * invD.x;
            float t0y = (nd.box.mn.y - o.y) * invD.y, t1y = (nd.box.mx.y - o.y) * invD.y;
            float t0z = (nd.box.mn.z - o.z) * invD.z, t1z = (nd.box.mx.z - o.z) * invD.z;
            float tmin = std::max(std::max(std::min(t0x,t1x), std::min(t0y,t1y)), std::min(t0z,t1z));
            float tmax = std::min(std::min(std::max(t0x,t1x), std::max(t0y,t1y)), std::max(t0z,t1z));
            tmin = std::max(tmin, 0.001f); tmax = std::min(tmax, closest);
            if (tmax < tmin) continue;
            if (nd.count > 0) {                        
                Hit rec;
                for (int i = nd.first; i < nd.first + nd.count; ++i) {
                    if (sphere_hit(spheres[idx[i]], o, d, 0.001f, closest, rec)) {
                        closest = rec.t; best = rec; found = true;
                    }
                }
            } else { stack[sp++] = nd.left; stack[sp++] = nd.right; }
        }
        return found;
    }
};

struct Camera {
    Vec3 origin, lower_left, horizontal, vertical, u, v;
    float lens_radius;
    Camera(Vec3 from, Vec3 at, Vec3 vup, float vfov_deg, float aspect,
           float aperture, float focus_dist) {
        float theta = vfov_deg * 3.14159265f / 180.0f;
        float h = std::tan(theta / 2.0f);
        float vh = 2.0f * h, vw = aspect * vh;
        Vec3 w = norm(from - at);
        u = norm(cross(vup, w));
        v = cross(w, u);
        origin = from;
        horizontal = u * (focus_dist * vw);
        vertical   = v * (focus_dist * vh);
        lower_left = origin - horizontal * 0.5f - vertical * 0.5f - w * focus_dist;
        lens_radius = aperture / 2.0f;
    }
    void ray(float s, float t, RNG& rng, Vec3& o, Vec3& d) const {
        Vec3 rd = random_in_unit_disk(rng) * lens_radius;
        Vec3 off = u * rd.x + v * rd.y;
        o = origin + off;
        d = lower_left + horizontal * s + vertical * t - origin - off;
    }
};

static inline Vec3 background(const Vec3& d) {
    float t = 0.5f * (norm(d).y + 1.0f);            // łagodne niebo
    return Vec3{1.0f, 1.0f, 1.0f} * (1.0f - t) + Vec3{0.5f, 0.7f, 1.0f} * t;
}

static Vec3 ray_color(Vec3 o, Vec3 d, const BVH& bvh, RNG& rng, int max_bounces) {
    Vec3 throughput{1, 1, 1}, radiance{0, 0, 0};
    for (int b = 0; b < max_bounces; ++b) {
        Hit h;
        if (!bvh.hit(o, d, h)) { radiance += throughput * background(d); break; }

        if (h.mat == LIGHT) { radiance += throughput * h.albedo; break; }

        if (h.mat == DIFFUSE) {
            d = cosine_sample_hemisphere(h.n, rng);   // pdf cancel -> *= albedo
            throughput *= h.albedo;
            o = h.p;
        } else if (h.mat == METAL) {
            Vec3 refl = reflect(norm(d), h.n) + random_in_unit_sphere(rng) * h.param;
            if (dot(refl, h.n) <= 0) break;            // pochłonięty
            throughput *= h.albedo; o = h.p; d = refl;
        } else { // GLASS
            float ratio = h.front ? (1.0f / h.param) : h.param;
            Vec3 unit = norm(d);
            float cos_t = std::min(dot(-unit, h.n), 1.0f);
            float sin_t = std::sqrt(1.0f - cos_t * cos_t);
            bool cannot_refract = ratio * sin_t > 1.0f;
            if (cannot_refract || schlick(cos_t, ratio) > rng.f())
                d = reflect(unit, h.n);
            else
                d = refract(unit, h.n, ratio);
            o = h.p; // throughput *= 1 (szkło bez absorpcji)
        }

        if (b > 3) {
            float p = std::max(throughput.x, std::max(throughput.y, throughput.z));
            if (rng.f() > p) break;
            throughput *= (1.0f / p);
        }
    }
    return radiance;
}

static inline Vec3 aces(const Vec3& x) {            // filmic tonemap
    const float a = 2.51f, b = 0.03f, c = 2.43f, dd = 0.59f, e = 0.14f;
    auto f = [&](float v) {
        float r = (v * (a * v + b)) / (v * (c * v + dd) + e);
        return r < 0 ? 0.f : (r > 1 ? 1.f : r);
    };
    return {f(x.x), f(x.y), f(x.z)};
}
static void save_ppm(const std::string& fn, const std::vector<Vec3>& fb, int W, int H) {
    FILE* f = std::fopen(fn.c_str(), "wb");
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; ++i) {
        Vec3 c = aces(fb[i]);
        unsigned char r = (unsigned char)(255.99f * std::pow(c.x, 1.0f/2.2f));
        unsigned char g = (unsigned char)(255.99f * std::pow(c.y, 1.0f/2.2f));
        unsigned char bl= (unsigned char)(255.99f * std::pow(c.z, 1.0f/2.2f));
        unsigned char px[3] = {r, g, bl};
        std::fwrite(px, 1, 3, f);
    }
    std::fclose(f);
}

struct Config {
    int   width      = 800;
    int   height     = 600;
    int   spp        = 64;    
    int   max_bounce = 16;
    int   frames     = 60;    
    float vfov       = 25.0f;
    float aperture   = 0.08f; 
    float exposure   = 1.4f;
};

static std::vector<Sphere> build_scene() {
    std::vector<Sphere> s;
    RNG rng(2024, 7);
    s.push_back({{0, -1000, 0}, 1000, DIFFUSE, {0.5f, 0.5f, 0.5f}, 0}); // podłoże
    for (int a = -7; a < 7; ++a)
        for (int b = -7; b < 7; ++b) {
            float choose = rng.f();
            Vec3 c{a + 0.9f * rng.f(), 0.2f, b + 0.9f * rng.f()};
            if (len(c - Vec3{4, 0.2f, 0}) <= 0.9f) continue;
            if (choose < 0.7f) {                       // matowe
                Vec3 alb{rng.f()*rng.f(), rng.f()*rng.f(), rng.f()*rng.f()};
                s.push_back({c, 0.2f, DIFFUSE, alb, 0});
            } else if (choose < 0.88f) {               // metal
                Vec3 alb{rng.f(0.5f,1), rng.f(0.5f,1), rng.f(0.5f,1)};
                s.push_back({c, 0.2f, METAL, alb, rng.f(0, 0.4f)});
            } else if (choose < 0.95f) {               // szkło
                s.push_back({c, 0.2f, GLASS, {1,1,1}, 1.5f});
            } else {                                   // świecące
                Vec3 e{rng.f(2,5), rng.f(2,5), rng.f(2,5)};
                s.push_back({c, 0.2f, LIGHT, e, 0});
            }
        }
    s.push_back({{0, 1, 0},  1.0f, GLASS,   {1, 1, 1},        1.5f});
    s.push_back({{-4, 1, 0}, 1.0f, DIFFUSE, {0.4f, 0.2f, 0.1f}, 0});
    s.push_back({{4, 1, 0},  1.0f, METAL,   {0.7f, 0.6f, 0.5f}, 0.0f});
    return s;
}

int main(int argc, char** argv) {
    Config cfg;
    if (argc >= 3) { cfg.width = std::atoi(argv[1]); cfg.height = std::atoi(argv[2]); }
    if (argc >= 4) cfg.spp    = std::atoi(argv[3]);
    if (argc >= 5) cfg.frames = std::atoi(argv[4]);

    const int W = cfg.width, H = cfg.height;
    BVH bvh; bvh.build(build_scene());
    printf("Scena: %zu sfer, BVH: %zu wezlow\n", bvh.spheres.size(), bvh.nodes.size());

    unsigned hw = std::thread::hardware_concurrency(); if (!hw) hw = 4;
    printf("Render %dx%d, spp=%d, klatki=%d, watki=%u\n", W, H, cfg.spp, cfg.frames, hw);

    auto t_all = std::chrono::high_resolution_clock::now();

    for (int frame = 0; frame < cfg.frames; ++frame) {
        std::vector<Vec3> fb(W * H);
        float ang = frame * (2.0f * 3.14159265f / cfg.frames);
        Vec3 from{13.0f * std::cos(ang), 2.0f, 13.0f * std::sin(ang)};
        Vec3 at{0, 1, 0};
        float focus = len(from - at);
        Camera cam(from, at, {0, 1, 0}, cfg.vfov, float(W) / H, cfg.aperture, focus);

        std::atomic<int> next_row{0};
        auto worker = [&]() {
            for (;;) {
                int y = next_row.fetch_add(1);
                if (y >= H) break;
                RNG rng(uint64_t(y) * 9781 + 1, uint64_t(frame) * 2654435761u + 1);
                for (int x = 0; x < W; ++x) {
                    Vec3 col{0, 0, 0};
                    for (int sidx = 0; sidx < cfg.spp; ++sidx) {
                        float u = (x + rng.f()) / (W - 1);
                        float v = (H - 1 - y + rng.f()) / (H - 1); // flip Y
                        Vec3 o, d; cam.ray(u, v, rng, o, d);
                        col += ray_color(o, d, bvh, rng, cfg.max_bounce);
                    }
                    fb[y * W + x] = col * (cfg.exposure / cfg.spp);
                }
            }
        };
        std::vector<std::thread> pool;
        for (unsigned i = 0; i < hw; ++i) pool.emplace_back(worker);
        for (auto& t : pool) t.join();

        char fn[64];
        std::snprintf(fn, sizeof fn, "frame_%03d.ppm", frame);
        save_ppm(fn, fb, W, H);
        printf("\rKlatka %d/%d gotowa", frame + 1, cfg.frames); fflush(stdout);
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration<double>(t_end - t_all).count();
    printf("\nGotowe w %.1f s (%.2f s/klatke)\n", secs, secs / cfg.frames);
    printf("GIF:  ffmpeg -framerate 24 -i frame_%%03d.ppm -vf "
           "\"scale=600:-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse\" out.gif\n");
    printf("MP4:  ffmpeg -framerate 24 -i frame_%%03d.ppm -c:v libx264 -pix_fmt yuv420p out.mp4\n");
    return 0;
}
