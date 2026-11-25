#include "Lighting.h"

#include <cmath>

#include "Art.h"
#include "rlgl.h"

namespace {

// The final pass: bloom from two blurred copies of the canvas, a split-tone grade, and a vignette.
const char* POST_SHADER = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D bloomNear;
uniform sampler2D bloomWide;
uniform vec3 shadowTint;
uniform vec3 highlightTint;
uniform float saturation;
uniform float bloomAmount;
uniform float vignetteAmount;
out vec4 finalColor;
void main() {
    vec3 c = texture(texture0, fragTexCoord).rgb;
    vec3 nearGlow = texture(bloomNear, fragTexCoord).rgb;
    vec3 wideGlow = texture(bloomWide, fragTexCoord).rgb;
    vec3 glow = max(nearGlow - vec3(0.52), 0.0) * 1.1 + max(wideGlow - vec3(0.36), 0.0) * 0.9;
    c += glow * bloomAmount;
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(luma), c, saturation);
    c *= mix(shadowTint, highlightTint, smoothstep(0.04, 0.85, luma));
    vec2 d = (fragTexCoord - vec2(0.5)) * vec2(1.0, 0.78);
    c *= 1.0 - vignetteAmount * smoothstep(0.26, 0.74, length(d));
    finalColor = vec4(c, 1.0);
}
)";

RenderTexture2D makeTarget(int width, int height, int filter) {
    RenderTexture2D target = LoadRenderTexture(width, height);
    SetTextureFilter(target.texture, filter);
    return target;
}

// Copies one render texture into another, scaled, with blending off (the copy is exact).
void copyTarget(const RenderTexture2D& from, const RenderTexture2D& to) {
    BeginTextureMode(to);
    rlDisableColorBlend();
    DrawTexturePro(from.texture, Rectangle{0, 0, (float)from.texture.width, -(float)from.texture.height},
                   Rectangle{0, 0, (float)to.texture.width, (float)to.texture.height}, Vector2{0, 0}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndTextureMode();
}

} // namespace

void SceneRenderer::load() {
    canvas = makeTarget(CANVAS_WIDTH, CANVAS_HEIGHT, TEXTURE_FILTER_POINT);
    reflection = makeTarget(CANVAS_WIDTH, CANVAS_HEIGHT, TEXTURE_FILTER_POINT);
    lightMap = makeTarget(CANVAS_WIDTH, CANVAS_HEIGHT, TEXTURE_FILTER_BILINEAR);
    bloomHalf = makeTarget(CANVAS_WIDTH / 2, CANVAS_HEIGHT / 2, TEXTURE_FILTER_BILINEAR);
    bloomQuarter = makeTarget(CANVAS_WIDTH / 4, CANVAS_HEIGHT / 4, TEXTURE_FILTER_BILINEAR);
    bloomEighth = makeTarget(CANVAS_WIDTH / 8, CANVAS_HEIGHT / 8, TEXTURE_FILTER_BILINEAR);

    // A soft round light: bright in the middle, fading smoothly to nothing at the edge.
    const int size = 64;
    Image image = GenImageColor(size, size, Color{255, 255, 255, 0});
    Color* pixels = (Color*)image.data;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (x + 0.5f) / size * 2.0f - 1.0f;
            float dy = (y + 0.5f) / size * 2.0f - 1.0f;
            float d = std::sqrt(dx * dx + dy * dy);
            float falloff = (d >= 1.0f) ? 0.0f : (1.0f - d) * (1.0f - d) * (0.6f + 0.4f * (1.0f - d));
            pixels[y * size + x].a = (unsigned char)std::lround(falloff * 255.0f);
        }
    }
    lightSprite = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(lightSprite, TEXTURE_FILTER_BILINEAR);

    post = LoadShaderFromMemory(nullptr, POST_SHADER);
    postReady = post.id != 0 && post.id != rlGetShaderIdDefault();
    if (postReady) {
        locBloomNear = GetShaderLocation(post, "bloomNear");
        locBloomWide = GetShaderLocation(post, "bloomWide");
        locShadow = GetShaderLocation(post, "shadowTint");
        locHighlight = GetShaderLocation(post, "highlightTint");
        locSaturation = GetShaderLocation(post, "saturation");
        locBloom = GetShaderLocation(post, "bloomAmount");
        locVignette = GetShaderLocation(post, "vignetteAmount");
    } else {
        TraceLog(LOG_WARNING, "Post-process shader unavailable: showing the scene without bloom and grading");
    }
}

void SceneRenderer::unload() {
    UnloadRenderTexture(canvas);
    UnloadRenderTexture(reflection);
    UnloadRenderTexture(lightMap);
    UnloadRenderTexture(bloomHalf);
    UnloadRenderTexture(bloomQuarter);
    UnloadRenderTexture(bloomEighth);
    UnloadTexture(lightSprite);
    if (postReady) UnloadShader(post);
    postReady = false;
}

void SceneRenderer::drawSoftLight(Vector2 center, float radius, Color color, float alpha) const {
    if (radius <= 0.0f || alpha <= 0.0f) return;
    Color tint = color;
    tint.a = (unsigned char)std::lround(std::fmin(1.0f, alpha) * 255.0f);
    DrawTexturePro(lightSprite, Rectangle{0, 0, (float)lightSprite.width, (float)lightSprite.height},
                   Rectangle{center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f}, Vector2{0, 0}, 0.0f, tint);
}

void SceneRenderer::renderLightMap(const std::vector<Light>& lights, const Mood& mood) const {
    BeginTextureMode(lightMap);
    ClearBackground(mood.groundLight);
    DrawRectangle(0, 0, CANVAS_WIDTH, mood.lightFrom, mood.skyLight);
    DrawRectangleGradientV(0, mood.lightFrom, CANVAS_WIDTH, mood.lightTo - mood.lightFrom, mood.skyLight, mood.groundLight);
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Light& light : lights) drawSoftLight(light.position, light.radius, light.color, light.strength);
    EndBlendMode();
    EndTextureMode();
}

void SceneRenderer::applyLightMap() const {
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTexturePro(lightMap.texture, Rectangle{0, 0, (float)CANVAS_WIDTH, -(float)CANVAS_HEIGHT},
                   Rectangle{0, 0, (float)CANVAS_WIDTH, (float)CANVAS_HEIGHT}, Vector2{0, 0}, 0.0f, WHITE);
    EndBlendMode();
}

void SceneRenderer::drawGlows(const std::vector<Light>& lights) const {
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Light& light : lights) {
        if (light.glow > 0.0f) drawSoftLight(light.position, light.glowRadius, light.color, light.glow);
    }
    EndBlendMode();
}

void SceneRenderer::copyCanvasToReflection() const {
    copyTarget(canvas, reflection);
}

// Blurred copies for the bloom: each step halves the size and averages neighboring pixels.
// These are texture passes of their own, so they run before the frame is drawn.
void SceneRenderer::prepareBloom() const {
    if (!postReady) return;
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    copyTarget(canvas, bloomHalf);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
    copyTarget(bloomHalf, bloomQuarter);
    copyTarget(bloomQuarter, bloomEighth);
}

void SceneRenderer::present(const Mood& mood, int width, int height) const {
    Rectangle source = {0.0f, 0.0f, (float)CANVAS_WIDTH, -(float)CANVAS_HEIGHT}; // render textures are stored upside down
    Rectangle dest = {0.0f, 0.0f, (float)width, (float)height};
    if (!postReady) {
        rlDrawRenderBatchActive();
        rlDisableColorBlend();
        DrawTexturePro(canvas.texture, source, dest, Vector2{0, 0}, 0.0f, WHITE);
        rlDrawRenderBatchActive();
        rlEnableColorBlend();
        return;
    }

    float shadow[3] = {mood.shadowTint.x, mood.shadowTint.y, mood.shadowTint.z};
    float highlight[3] = {mood.highlightTint.x, mood.highlightTint.y, mood.highlightTint.z};
    BeginShaderMode(post);
    SetShaderValueTexture(post, locBloomNear, bloomQuarter.texture);
    SetShaderValueTexture(post, locBloomWide, bloomEighth.texture);
    SetShaderValue(post, locShadow, shadow, SHADER_UNIFORM_VEC3);
    SetShaderValue(post, locHighlight, highlight, SHADER_UNIFORM_VEC3);
    SetShaderValue(post, locSaturation, &mood.saturation, SHADER_UNIFORM_FLOAT);
    SetShaderValue(post, locBloom, &mood.bloom, SHADER_UNIFORM_FLOAT);
    SetShaderValue(post, locVignette, &mood.vignette, SHADER_UNIFORM_FLOAT);
    DrawTexturePro(canvas.texture, source, dest, Vector2{0, 0}, 0.0f, WHITE);
    EndShaderMode();
}
