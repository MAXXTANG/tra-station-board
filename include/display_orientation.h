#pragma once
#include <stdint.h>

namespace display {
constexpr int WIDTH = 320;
constexpr int HEIGHT = 170;

constexpr uint8_t landscapeRotation(bool upsideDown) {
    return upsideDown ? 3 : 1;
}

// The touch controller always reports portrait coordinates, regardless of LCD rotation.
inline bool mapTouch(int rawX, int rawY, bool upsideDown, int &x, int &y) {
    if (rawX < 0 || rawX >= HEIGHT || rawY < 0 || rawY >= WIDTH) return false;
    x = rawY;
    y = HEIGHT - 1 - rawX;
    if (upsideDown) {
        x = WIDTH - 1 - x;
        y = HEIGHT - 1 - y;
    }
    return true;
}
}
