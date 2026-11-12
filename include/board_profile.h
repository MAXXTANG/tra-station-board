#pragma once
#include <stdint.h>

namespace board {
#ifdef TRA_BOARD_CYD
constexpr bool CYD=true;
constexpr int WIDTH=320, HEIGHT=240, ROWS=5, ROW_HEIGHT=32;
constexpr int STATUS_Y=190, NAV_Y=208, PICKER_STEP=40;
constexpr int BUTTON=0, BACKLIGHT=21;
constexpr const char *NAME="ESP32-2432S028R";
#else
constexpr bool CYD=false;
constexpr int WIDTH=320, HEIGHT=170, ROWS=3, ROW_HEIGHT=29;
constexpr int STATUS_Y=119, NAV_Y=138, PICKER_STEP=26;
constexpr int BUTTON=14, BACKLIGHT=38;
constexpr const char *NAME="LILYGO T-Display-S3 Touch";
#endif
constexpr int TABLE_Y=29;
constexpr int TABLE_BOTTOM=TABLE_Y+ROWS*ROW_HEIGHT;
constexpr int NAV_TEXT_Y=NAV_Y+9;
constexpr int PICKER_BOTTOM=NAV_Y+2;
constexpr int PICKER_TEXT_Y=PICKER_BOTTOM+7;
}
