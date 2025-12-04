#pragma once

#include <cstdint>

// Component definitions for visual timeline demo
struct Position {
    double x, y;
};

struct Velocity {
    double x, y;
};

struct Health {
    float value;
    float max_value;
};

struct RenderColor {
    uint32_t color;
    float radius;
};

struct FoodSprite {
    int sprite_index;  // Index into the food sprites array
    float scale;       // Size multiplier for the sprite
};
