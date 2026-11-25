#include "Display.h"

#include <algorithm>
#include <cmath>

#include "rlgl.h"

namespace {

// Sharp bilinear scaling, for screens larger than the frame. Each frame pixel keeps its color
// across the screen pixels it covers, and only the screen pixel on its edge blends with the
// neighbor, so the picture shows neither blur nor pixels of uneven width.
const char* SHARP_SHADER = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float scale;
out vec4 finalColor;
void main() {
    vec2 sourceSize = vec2(textureSize(texture0, 0));
    vec2 texel = fragTexCoord * sourceSize;
    vec2 base = floor(texel - 0.5) + 0.5;
    vec2 blend = clamp((texel - base - 0.5) * scale + 0.5, 0.0, 1.0);
    finalColor = vec4(texture(texture0, (base + blend) / sourceSize).rgb, 1.0);
}
)";

constexpr int WINDOW_FPS = 60; // the frame cap in the window (main sets the same at the start)

} // namespace

void Display::load() {
    sharp = LoadShaderFromMemory(nullptr, SHARP_SHADER);
    sharpReady = sharp.id != 0 && sharp.id != rlGetShaderIdDefault();
    if (sharpReady) locScale = GetShaderLocation(sharp, "scale");
}

void Display::unload() {
    if (frame.id != 0) UnloadRenderTexture(frame);
    frame = {};
    frameScale = 0;
    if (sharpReady) UnloadShader(sharp);
    sharpReady = false;
}

float Display::fitScale() const {
    return std::fmin((float)GetRenderWidth() / SCREEN_WIDTH, (float)GetRenderHeight() / SCREEN_HEIGHT);
}

void Display::setFullscreen(bool on) {
    if (on == fullscreen) return;
    if (on) {
        const int monitor = GetCurrentMonitor(); // the monitor the window is on
        const int monitorWidth = GetMonitorWidth(monitor);
        const int monitorHeight = GetMonitorHeight(monitor);
        if (monitorWidth <= 0 || monitorHeight <= 0) {
            TraceLog(LOG_WARNING, "DISPLAY: the monitor's resolution is unknown; staying in the window");
            return;
        }
        windowPosition = GetWindowPosition();
        // raylib's fullscreen uses the window's size as the video mode, so make it the monitor's
        // own resolution first: the screen keeps its resolution, and the frame is scaled to it.
        SetWindowSize(monitorWidth, monitorHeight);
        ToggleFullscreen();
        fullscreen = IsWindowFullscreen();
        if (!fullscreen) { // the monitor could not be taken: back to the window as it was
            SetWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
            SetWindowPosition((int)windowPosition.x, (int)windowPosition.y);
            TraceLog(LOG_WARNING, "DISPLAY: fullscreen is unavailable; staying in the window");
            return;
        }
        HideCursor(); // nothing in the game uses the mouse

        // The frame is drawn one whole step larger than the screen needs (2x for 1920x1080, 3x for
        // 2560x1440), up to 3x, and scaled down: that smooths text and shapes, and the art stays exact.
        frameScale = std::clamp((int)std::floor(fitScale() + 0.001f) + 1, 1, 3);
        frame = LoadRenderTexture(SCREEN_WIDTH * frameScale, SCREEN_HEIGHT * frameScale);
        if (frame.id == 0) TraceLog(LOG_WARNING, "DISPLAY: no off-screen frame; drawing straight to the screen");
    } else {
        ToggleFullscreen(); // a window again first: resizing while fullscreen would change the video mode
        SetWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
        SetWindowPosition((int)windowPosition.x, (int)windowPosition.y);
        ShowCursor();
        fullscreen = false;
        if (frame.id != 0) UnloadRenderTexture(frame);
        frame = {};
        frameScale = 0;
    }
    // Fullscreen may run the monitor at a faster refresh rate than the desktop. With V-Sync, the
    // frame cap must keep up with it, or frames would wait an extra refresh and motion would
    // stutter. (Movement is integrated exactly, so the game plays the same at any frame rate.)
    const int hz = refreshRate();
    SetTargetFPS((fullscreen && hz > 0) ? hz : WINDOW_FPS);
    TraceLog(LOG_INFO, "DISPLAY: %s, %d x %d at %d Hz (frame drawn at %dx)", fullscreen ? "fullscreen" : "window", width(),
             height(), hz, fullscreen ? frameScale : 1);
}

int Display::width() const {
    return GetRenderWidth();
}

int Display::height() const {
    return GetRenderHeight();
}

int Display::refreshRate() const {
    return GetMonitorRefreshRate(GetCurrentMonitor());
}

void Display::beginFrame() const {
    ClearBackground(BLACK);
    if (!fullscreen) return; // the window is exactly the frame: draw straight to it, as always
    if (frame.id != 0) {
        BeginTextureMode(frame);
        ClearBackground(BLACK);
        rlPushMatrix();
        rlScalef((float)frameScale, (float)frameScale, 1.0f); // frame coordinates -> the larger frame
        return;
    }
    // No off-screen frame: draw straight to the screen, scaled and centered.
    const float scale = fitScale();
    rlPushMatrix();
    rlTranslatef(std::floor((GetRenderWidth() - SCREEN_WIDTH * scale) / 2.0f), std::floor((GetRenderHeight() - SCREEN_HEIGHT * scale) / 2.0f), 0.0f);
    rlScalef(scale, scale, 1.0f);
}

void Display::endFrame() const {
    if (!fullscreen) return;
    rlPopMatrix();
    if (frame.id == 0) return;
    EndTextureMode();

    // The largest 16:9 area that fits the screen, centered, with black bars around it.
    const float scale = fitScale();
    const float w = std::floor(SCREEN_WIDTH * scale);
    const float h = std::floor(SCREEN_HEIGHT * scale);
    const Rectangle dest = {std::floor((GetRenderWidth() - w) / 2.0f), std::floor((GetRenderHeight() - h) / 2.0f), w, h};
    const float ratio = w / (float)frame.texture.width; // screen pixels per frame pixel
    const bool exact = std::fabs(ratio - 1.0f) < 0.001f;
    const bool sharpen = ratio > 1.001f && sharpReady; // only above 3x
    SetTextureFilter(frame.texture, exact ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);

    rlDrawRenderBatchActive();
    rlDisableColorBlend(); // an exact copy: the frame's alpha channel is not transparency
    if (sharpen) {
        BeginShaderMode(sharp);
        SetShaderValue(sharp, locScale, &ratio, SHADER_UNIFORM_FLOAT);
    }
    // Render textures are stored upside down, so the source height is negative.
    DrawTexturePro(frame.texture, Rectangle{0.0f, 0.0f, (float)frame.texture.width, -(float)frame.texture.height}, dest,
                   Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    if (sharpen) {
        EndShaderMode(); // draws the frame with the shader
    } else {
        rlDrawRenderBatchActive();
    }
    rlEnableColorBlend();
}
