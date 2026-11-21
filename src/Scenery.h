#pragma once

#include "World.h"

// Decoration only: no collision, no nodes, no links.
// Both functions draw in screen space and use the camera's x position for parallax.

// Sky, warm light, distant hills, mid-distance props, light motes, and region landmarks.
void drawBackdrop(const World& world, float cameraX, float time);

// Dark plants and rocks along the bottom edge of the screen.
void drawForeground(const World& world, float cameraX, float time);
