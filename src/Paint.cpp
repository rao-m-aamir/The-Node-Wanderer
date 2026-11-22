#include "Paint.h"

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Pictures
// ---------------------------------------------------------------------------

Picture::Picture(int w, int h) : width(w), height(h), pixels((size_t)w * h, Color{0, 0, 0, 0}) {}

void Picture::blend(int x, int y, Color color, float amount) {
    if (!inside(x, y)) return;
    float a = color.a / 255.0f * amount;
    if (a <= 0.0f) return;
    if (a > 1.0f) a = 1.0f;
    Color& d = pixels[y * width + x];
    float da = d.a / 255.0f;
    float oa = a + da * (1.0f - a);
    auto channel = [&](unsigned char s, unsigned char t) {
        return (unsigned char)std::lround((s * a + t * da * (1.0f - a)) / oa);
    };
    d = Color{channel(color.r, d.r), channel(color.g, d.g), channel(color.b, d.b), (unsigned char)std::lround(oa * 255.0f)};
}

void Picture::rect(int x, int y, int w, int h, Color color) {
    for (int yy = std::max(0, y); yy < std::min(height, y + h); yy++) {
        for (int xx = std::max(0, x); xx < std::min(width, x + w); xx++) pixels[yy * width + xx] = color;
    }
}

static Texture2D uploadPixels(const std::vector<Color>& data, int width, int height) {
    Image image = {};
    image.data = (void*)data.data();
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Texture2D texture = LoadTextureFromImage(image); // copies the pixels to the GPU
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

Texture2D Picture::upload() const {
    return uploadPixels(pixels, width, height);
}

Texture2D Picture::uploadMask() const {
    std::vector<Color> mask(pixels.size());
    for (size_t i = 0; i < pixels.size(); i++) {
        mask[i] = Color{255, 255, 255, (unsigned char)(pixels[i].a > 0 ? 255 : 0)};
    }
    return uploadPixels(mask, width, height);
}

// ---------------------------------------------------------------------------
// Masks
// ---------------------------------------------------------------------------

Mask::Mask(int w, int h) : width(w), height(h), in((size_t)w * h, 0), depth((size_t)w * h, 0.0f) {}

bool Mask::at(int x, int y) const {
    return x >= 0 && y >= 0 && x < width && y < height && in[y * width + x] != 0;
}

void Mask::set(int x, int y, bool on) {
    if (x >= 0 && y >= 0 && x < width && y < height) in[y * width + x] = on ? 1 : 0;
}

void Mask::span(int y, int x0, int x1, bool on) {
    if (y < 0 || y >= height) return;
    for (int x = std::max(0, x0); x < std::min(width, x1); x++) in[y * width + x] = on ? 1 : 0;
}

void Mask::rect(int x, int y, int w, int h, bool on) {
    for (int yy = y; yy < y + h; yy++) span(yy, x, x + w, on);
}

void Mask::ellipse(float cx, float cy, float rx, float ry, bool on) {
    if (rx <= 0.0f || ry <= 0.0f) return;
    for (int y = (int)std::floor(cy - ry - 1); y <= (int)std::ceil(cy + ry + 1); y++) {
        float dy = (y + 0.5f - cy) / ry;
        if (dy < -1.0f || dy > 1.0f) continue;
        float half = rx * std::sqrt(1.0f - dy * dy);
        span(y, (int)std::ceil(cx - half - 0.5f), (int)std::floor(cx + half - 0.5f) + 1, on);
    }
}

void Mask::blob(float cx, float cy, float r, int seed, float roughness, bool on) {
    float reach = r * (1.0f + roughness) + 1.0f;
    for (int y = (int)std::floor(cy - reach); y <= (int)std::ceil(cy + reach); y++) {
        for (int x = (int)std::floor(cx - reach); x <= (int)std::ceil(cx + reach); x++) {
            float dx = x + 0.5f - cx;
            float dy = y + 0.5f - cy;
            float angle = std::atan2(dy, dx);
            float lump = fbm2(std::cos(angle) * 1.6f + 7.0f, std::sin(angle) * 1.6f + 3.0f, seed, 3) - 0.5f;
            float edge = r * (1.0f + roughness * 2.0f * lump);
            if (dx * dx + dy * dy < edge * edge) set(x, y, on);
        }
    }
}

void Mask::capsule(Vector2 a, Vector2 b, float radiusA, float radiusB, bool on) {
    float reach = std::max(radiusA, radiusB) + 1.0f;
    int x0 = (int)std::floor(std::min(a.x, b.x) - reach), x1 = (int)std::ceil(std::max(a.x, b.x) + reach);
    int y0 = (int)std::floor(std::min(a.y, b.y) - reach), y1 = (int)std::ceil(std::max(a.y, b.y) + reach);
    float vx = b.x - a.x, vy = b.y - a.y;
    float lengthSq = vx * vx + vy * vy;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float px = x + 0.5f - a.x, py = y + 0.5f - a.y;
            float t = (lengthSq > 0.0f) ? std::clamp((px * vx + py * vy) / lengthSq, 0.0f, 1.0f) : 0.0f;
            float dx = px - vx * t, dy = py - vy * t;
            float r = radiusA + (radiusB - radiusA) * t;
            if (dx * dx + dy * dy < r * r) set(x, y, on);
        }
    }
}

void Mask::polygon(const Vector2* points, int count, bool on) {
    if (count < 3) return;
    float top = points[0].y, bottom = points[0].y;
    for (int i = 1; i < count; i++) {
        top = std::min(top, points[i].y);
        bottom = std::max(bottom, points[i].y);
    }
    std::vector<float> crossings;
    for (int y = (int)std::floor(top); y <= (int)std::ceil(bottom); y++) {
        float sy = y + 0.5f;
        crossings.clear();
        for (int i = 0; i < count; i++) {
            Vector2 p = points[i], q = points[(i + 1) % count];
            if ((p.y <= sy && q.y > sy) || (q.y <= sy && p.y > sy)) {
                crossings.push_back(p.x + (sy - p.y) / (q.y - p.y) * (q.x - p.x));
            }
        }
        std::sort(crossings.begin(), crossings.end());
        for (size_t i = 0; i + 1 < crossings.size(); i += 2) {
            span(y, (int)std::ceil(crossings[i] - 0.5f), (int)std::floor(crossings[i + 1] - 0.5f) + 1, on);
        }
    }
}

void Mask::measure() {
    const float big = 1e6f;
    const float diagonal = 1.4142f;
    for (size_t i = 0; i < in.size(); i++) depth[i] = in[i] ? big : 0.0f;
    auto value = [&](int x, int y) { // past the border counts as deep inside
        if (x < 0 || y < 0 || x >= width || y >= height) return big;
        return depth[y * width + x];
    };
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float& d = depth[y * width + x];
            if (d == 0.0f) continue;
            d = std::min({d, value(x - 1, y) + 1.0f, value(x, y - 1) + 1.0f, value(x - 1, y - 1) + diagonal,
                          value(x + 1, y - 1) + diagonal});
        }
    }
    for (int y = height - 1; y >= 0; y--) {
        for (int x = width - 1; x >= 0; x--) {
            float& d = depth[y * width + x];
            if (d == 0.0f) continue;
            d = std::min({d, value(x + 1, y) + 1.0f, value(x, y + 1) + 1.0f, value(x + 1, y + 1) + diagonal,
                          value(x - 1, y + 1) + diagonal});
        }
    }
}

Vector2 Mask::outward(int x, int y) const {
    auto value = [&](int xx, int yy) {
        if (xx < 0 || yy < 0 || xx >= width || yy >= height) return depth[y * width + x];
        return std::min(depth[yy * width + xx], 12.0f);
    };
    float gx = value(x + 1, y) - value(x - 1, y) + 0.5f * (value(x + 1, y - 1) - value(x - 1, y - 1) + value(x + 1, y + 1) - value(x - 1, y + 1));
    float gy = value(x, y + 1) - value(x, y - 1) + 0.5f * (value(x - 1, y + 1) - value(x - 1, y - 1) + value(x + 1, y + 1) - value(x + 1, y - 1));
    float length = std::sqrt(gx * gx + gy * gy);
    if (length < 0.001f) return Vector2{0.0f, -1.0f};
    return Vector2{-gx / length, -gy / length}; // depth grows inward, so outward is against the gradient
}

// ---------------------------------------------------------------------------
// Shading
// ---------------------------------------------------------------------------

void shade(Picture& out, const Mask& mask, const Shading& s, int offsetX, int offsetY) {
    for (int y = 0; y < mask.height; y++) {
        for (int x = 0; x < mask.width; x++) {
            if (!mask.in[y * mask.width + x]) continue;
            int px = x + offsetX, py = y + offsetY;
            if (!out.inside(px, py)) continue;
            float d = mask.depth[y * mask.width + x];
            float value = s.base;
            float facing = 0.0f;
            if (d <= s.bevel) {
                Vector2 n = mask.outward(x, y);
                facing = n.x * s.toLight.x + n.y * s.toLight.y;
                float reach = 1.0f - (d - 1.0f) / s.bevel;
                value += s.rim * facing * reach;
                if (s.topLight > 0.0f && n.y < -0.4f) value += s.topLight * (-n.y) * reach;
            }
            value += (fbm2(px * s.grainScale, py * s.grainScale, s.seed, 3) - 0.5f) * 2.0f * s.grain;
            if (py > s.darkenFrom) value -= s.darkenAmount * std::min(1.0f, (py - s.darkenFrom) / s.darkenRange);
            Color c = s.dither ? pickRamp(s.ramp, s.steps, value, px, py)
                               : s.ramp[std::clamp((int)std::lround(std::clamp(value, 0.0f, 1.0f) * (s.steps - 1)), 0, s.steps - 1)];
            if (s.outline && d <= 1.01f && facing < -0.25f) c = s.ramp[0];
            out.set(px, py, c);
        }
    }
}

// ---------------------------------------------------------------------------
// Noise, dithering, color
// ---------------------------------------------------------------------------

static float lattice(int x, int y, int seed) {
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u + (unsigned int)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFFu) / 16777216.0f;
}

float noise2(float x, float y, int seed) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy;
    float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    float a = lattice(ix, iy, seed), b = lattice(ix + 1, iy, seed);
    float c = lattice(ix, iy + 1, seed), d = lattice(ix + 1, iy + 1, seed);
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy;
}

float fbm2(float x, float y, int seed, int octaves) {
    float sum = 0.0f, amplitude = 0.5f, total = 0.0f;
    for (int i = 0; i < octaves; i++) {
        sum += noise2(x, y, seed + i * 101) * amplitude;
        total += amplitude;
        x *= 2.03f;
        y *= 2.03f;
        amplitude *= 0.5f;
    }
    return sum / total;
}

float bayer4(int x, int y) {
    static const int matrix[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    return (matrix[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f;
}

Color pickRamp(const Color* ramp, int steps, float value, int x, int y) {
    value = std::clamp(value, 0.0f, 1.0f) * (steps - 1);
    int index = (int)std::floor(value);
    if (value - index > bayer4(x, y)) index++;
    return ramp[std::clamp(index, 0, steps - 1)];
}

Color mixColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{(unsigned char)std::lround(a.r + (b.r - a.r) * t), (unsigned char)std::lround(a.g + (b.g - a.g) * t),
                 (unsigned char)std::lround(a.b + (b.b - a.b) * t), (unsigned char)std::lround(a.a + (b.a - a.a) * t)};
}

Color scaleColor(Color c, float k) {
    auto channel = [k](unsigned char v) { return (unsigned char)std::clamp((int)std::lround(v * k), 0, 255); };
    return Color{channel(c.r), channel(c.g), channel(c.b), c.a};
}

void ditherGradient(Picture& p, int y0, int y1, Color top, Color bottom) {
    const int steps = 10; // how many bands the gradient is made of; dithering softens their edges
    for (int y = std::max(0, y0); y < std::min(p.height, y1); y++) {
        float t = (y1 > y0) ? (y - y0) / (float)(y1 - y0) * (steps - 1) : 0.0f;
        int band = (int)std::floor(t);
        for (int x = 0; x < p.width; x++) {
            int b = band + ((t - band) > bayer4(x, y) ? 1 : 0);
            p.pixels[y * p.width + x] = mixColor(top, bottom, std::min(b, steps - 1) / (float)(steps - 1));
        }
    }
}

void fogRows(Picture& p, Color fog, int y0, int y1, float amount0, float amount1) {
    for (int y = 0; y < p.height; y++) {
        float t = (y1 > y0) ? std::clamp((y - y0) / (float)(y1 - y0), 0.0f, 1.0f) : 0.0f;
        float amount = amount0 + (amount1 - amount0) * t;
        if (amount <= 0.0f) continue;
        for (int x = 0; x < p.width; x++) {
            Color& c = p.pixels[y * p.width + x];
            if (c.a == 0) continue;
            unsigned char alpha = c.a;
            c = mixColor(c, fog, amount);
            c.a = alpha;
        }
    }
}
