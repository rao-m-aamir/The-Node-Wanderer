#pragma once

#include "raylib.h"

// The game's frame: everything is drawn in these coordinates, whatever the window or screen is.
constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

// Where the frame is shown: in a 1280x720 window, or in exclusive fullscreen at the resolution
// of the monitor the window is on.
//
// In the window, the frame is drawn straight to the screen (with the window's anti-aliasing).
// In fullscreen it is drawn 2x or 3x larger into an off-screen frame, then scaled down to fit the
// screen, kept at 16:9 with black bars. Drawing larger and scaling down smooths the edges of text
// and shapes, and the scales are chosen so the pixel art stays exact: at 1920x1080 each art pixel
// is 3x3 screen pixels, at 2560x1440 4x4, and at 3840x2160 6x6.
class Display {
public:
    void load();   // after InitWindow
    void unload(); // before CloseWindow

    void setFullscreen(bool on); // exclusive fullscreen at the current monitor's resolution, or the window
    bool isFullscreen() const { return fullscreen; }
    int width() const;  // the resolution the frame is shown at (the window's, or the full screen's)
    int height() const;
    int refreshRate() const; // of the monitor in use, in Hz (0 if unknown)

    void beginFrame() const; // draw the frame (in 1280x720 coordinates)...
    void endFrame() const;   // ...then show it on the window or the screen

private:
    float fitScale() const; // screen pixels per frame unit, for the largest 16:9 area that fits

    RenderTexture2D frame = {}; // fullscreen only: the frame, drawn `frameScale` times larger
    int frameScale = 0;
    Shader sharp = {};          // for screens larger than the frame (above 3x): sharp bilinear
    bool sharpReady = false;
    int locScale = -1;
    bool fullscreen = false;
    Vector2 windowPosition = {}; // where the window was before fullscreen, to put it back there
};
