#pragma once

#include <vector>

#include "raylib.h"

// The scenery is painted pixel by pixel into pictures on the CPU when a region loads, then
// uploaded once as textures. Every picture is original and built into the program: there
// are no image files to load or license.
//
// The usual recipe: describe a shape in a Mask, call measure(), then shade() it with a
// palette ramp. Shading lights the edge that faces the light, darkens the edge that faces
// away, adds a little texture, and dithers between the ramp's steps like pixel art.

struct Picture {
    int width = 0;
    int height = 0;
    std::vector<Color> pixels; // row by row from the top-left; transparent where nothing is painted

    Picture() = default;
    Picture(int w, int h);

    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    Color get(int x, int y) const { return inside(x, y) ? pixels[y * width + x] : Color{0, 0, 0, 0}; }
    bool painted(int x, int y) const { return inside(x, y) && pixels[y * width + x].a > 0; }
    void set(int x, int y, Color color) {
        if (inside(x, y)) pixels[y * width + x] = color;
    }
    void blend(int x, int y, Color color, float amount = 1.0f); // paints over what is there
    void rect(int x, int y, int w, int h, Color color);

    Texture2D upload() const;     // as it is
    Texture2D uploadMask() const; // white wherever something is painted (tinted for mist veils)
};

// Which pixels belong to a shape, and how deep inside it each one is.
struct Mask {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> in;
    std::vector<float> depth; // distance to the nearest outside pixel; filled by measure()

    Mask(int w, int h);

    bool at(int x, int y) const;
    void set(int x, int y, bool on = true);
    void span(int y, int x0, int x1, bool on = true); // pixels x0 .. x1-1 of row y
    void rect(int x, int y, int w, int h, bool on = true);
    void ellipse(float cx, float cy, float rx, float ry, bool on = true);
    void circle(float cx, float cy, float r, bool on = true) { ellipse(cx, cy, r, r, on); }
    void blob(float cx, float cy, float r, int seed, float roughness = 0.3f, bool on = true); // a lumpy disc
    void capsule(Vector2 a, Vector2 b, float radiusA, float radiusB, bool on = true);       // a tapered stroke
    void polygon(const Vector2* points, int count, bool on = true);

    // Computes `depth`. Pixels past the picture's border count as inside, so shapes that run
    // off the edge (ground, cliffs, hills) get no rim there.
    void measure();
    Vector2 outward(int x, int y) const; // unit normal pointing out of the shape
};

struct Shading {
    const Color* ramp = nullptr; // dark to light
    int steps = 0;
    Vector2 toLight = {0.55f, -0.83f}; // unit vector pointing toward the light (y grows downward)
    float base = 0.45f;                // value of the flat inside
    float rim = 0.35f;                 // how much the edges facing toward / away from the light change it
    float bevel = 3.0f;                // how far the edge lighting reaches inside, in pixels
    float grain = 0.12f;               // texture noise
    float grainScale = 0.2f;
    float topLight = 0.0f;             // extra light on surfaces that face up
    float darkenFrom = 1e9f;           // picture row where the shape starts to sink into shadow
    float darkenRange = 100.0f;
    float darkenAmount = 0.3f;
    bool outline = true;               // the outer edge away from the light gets the darkest step
    bool dither = true;                // dither between ramp steps (false: nearest step, cleaner)
    int seed = 1;
};

// Shades the mask into the picture (mask pixel x, y lands on picture x + offsetX, y + offsetY).
void shade(Picture& out, const Mask& mask, const Shading& style, int offsetX = 0, int offsetY = 0);

// Smooth value noise in [0, 1], and several octaves of it.
float noise2(float x, float y, int seed);
float fbm2(float x, float y, int seed, int octaves = 4);

// Ordered dither threshold for a pixel, in [0, 1).
float bayer4(int x, int y);

// A color from a ramp for a value in [0, 1], dithered between neighboring steps.
Color pickRamp(const Color* ramp, int steps, float value, int x, int y);

Color mixColor(Color a, Color b, float t);
Color scaleColor(Color c, float k); // brighter (> 1) or darker (< 1), same alpha

// A vertical band of dithered color from `top` to `bottom` across the whole picture.
void ditherGradient(Picture& p, int y0, int y1, Color top, Color bottom);

// Mixes every painted pixel toward a fog color: more where `amount` is higher.
void fogRows(Picture& p, Color fog, int y0, int y1, float amount0, float amount1);
