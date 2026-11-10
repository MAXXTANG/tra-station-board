#include <cassert>
#include <iostream>
#include "display_orientation.h"

int main() {
    assert(display::landscapeRotation(false) == 1);
    assert(display::landscapeRotation(true) == 3);
    int x = -1, y = -1;
    // Physical portrait corners become the correct corners on each landscape orientation.
    assert(display::mapTouch(0, 0, false, x, y) && x == 0 && y == 169);
    assert(display::mapTouch(169, 319, false, x, y) && x == 319 && y == 0);
    assert(display::mapTouch(0, 0, true, x, y) && x == 319 && y == 0);
    assert(display::mapTouch(169, 319, true, x, y) && x == 0 && y == 169);

    // The same visible north/south/page/station targets must keep their logical coordinates.
    struct Target { int x, y; } targets[] = {{205,154},{279,154},{110,154},{100,45},{295,125}};
    for (auto target : targets) {
        int rawX = 169 - target.y, rawY = target.x;
        assert(display::mapTouch(rawX, rawY, false, x, y));
        assert(x == target.x && y == target.y);
        rawX = target.y; rawY = 319 - target.x;
        assert(display::mapTouch(rawX, rawY, true, x, y));
        assert(x == target.x && y == target.y);
    }
    for (int rawX = 0; rawX < 170; ++rawX) {
        for (int rawY = 0; rawY < 320; ++rawY) {
            int normalX,normalY;
            assert(display::mapTouch(rawX,rawY,false,normalX,normalY));
            assert(display::mapTouch(rawX,rawY,true,x,y));
            assert(x >= 0 && x < 320 && y >= 0 && y < 170);
            assert(normalX + x == 319 && normalY + y == 169);
        }
    }
    for (auto raw : {Target{-1,0}, Target{170,0}, Target{0,-1}, Target{0,320}}) {
        assert(!display::mapTouch(raw.x,raw.y,false,x,y));
        assert(!display::mapTouch(raw.x,raw.y,true,x,y));
    }
    std::cout << "PASS: orientation corners, all pixels, invalid bounds, and visible UI targets\n";
}
