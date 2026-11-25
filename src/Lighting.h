#pragma once

#include <vector>

#include "raylib.h"

// Light for the pixel canvas. Every frame:
//   1. the light map is drawn: sky light falling from above, plus a soft disc for each light;
//   2. the scene is drawn into the canvas and multiplied by the light map, so places far from
//      light sink into shadow;
//   3. each light also glows on its own (added on top);
//   4. the canvas is scaled up 2x through a small shader that adds bloom, a color grade, and
//      a vignette (into the 1280x720 frame, see Display.h). If the shader cannot be compiled,
//      the canvas is shown without it.

struct Light {
    Vector2 position;  // canvas pixels
    float radius;      // how far it lights the scene, canvas pixels
    Color color;
    float strength;    // 0..1 in the light map
    float glowRadius;  // size of its own halo; 0 = none
    float glow;        // 0..1 brightness of the halo
};

// How a region is lit and graded.
struct Mood {
    Color skyLight;       // light-map value at the top of the canvas (white leaves the sky as painted)
    Color groundLight;    // light-map value lower down, away from any light
    int lightFrom;        // canvas rows where the light map fades from sky to ground light
    int lightTo;
    Vector3 shadowTint;   // final grade: multiplier for dark pixels...
    Vector3 highlightTint; // ...and for bright ones
    float saturation;
    float bloom;
    float vignette;
};

class SceneRenderer {
public:
    void load();
    void unload();

    // Call outside any texture mode.
    void renderLightMap(const std::vector<Light>& lights, const Mood& mood) const;
    void copyCanvasToReflection() const; // a still copy of the canvas so far, for water to reflect
    void prepareBloom() const;           // the blurred copies of the finished canvas, for the bloom

    // Call while drawing into the frame: the canvas, scaled to it, with bloom and the grade.
    void present(const Mood& mood, int width, int height) const;

    // Call while drawing into the canvas.
    void applyLightMap() const;
    void drawGlows(const std::vector<Light>& lights) const;
    void drawSoftLight(Vector2 center, float radius, Color color, float alpha) const; // additive

    RenderTexture2D canvas = {};
    RenderTexture2D reflection = {};

private:
    RenderTexture2D lightMap = {};
    RenderTexture2D bloomHalf = {};
    RenderTexture2D bloomQuarter = {};
    RenderTexture2D bloomEighth = {};
    Texture2D lightSprite = {}; // white, alpha falls off softly from the middle
    Shader post = {};
    bool postReady = false;
    int locBloomNear = -1, locBloomWide = -1, locShadow = -1, locHighlight = -1;
    int locSaturation = -1, locBloom = -1, locVignette = -1;
};
