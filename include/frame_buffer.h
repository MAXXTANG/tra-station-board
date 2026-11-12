#pragma once
#include <TFT_eSPI.h>
#include "board_profile.h"

// CYD: one 320x24 RGB565 band (15 KiB). S3: the original PSRAM sprite.
// Coordinates remain screen coordinates so layout and touch use one space.
class BoardFrame {
    TFT_eSprite sprite;
    int top=0;
public:
    static constexpr int BAND_HEIGHT=board::CYD?24:board::HEIGHT;
    explicit BoardFrame(TFT_eSPI *tft):sprite(tft){}
    bool begin(){
        sprite.setColorDepth(16);sprite.setAttribute(PSRAM_ENABLE,!board::CYD);
        return sprite.createSprite(board::WIDTH,BAND_HEIGHT)!=nullptr;
    }
    void band(int y){top=y;}
    int bandTop()const{return top;}
    void fillSprite(uint16_t c){sprite.fillSprite(c);}
    void drawPixel(int x,int y,uint16_t c){
        if(y>=top && y<top+BAND_HEIGHT)sprite.drawPixel(x,y-top,c);
    }
    uint16_t readPixel(int x,int y){return sprite.readPixel(x,y-top);}
    void fillRect(int x,int y,int w,int h,uint16_t c){sprite.fillRect(x,y-top,w,h,c);}
    void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c){sprite.fillRoundRect(x,y-top,w,h,r,c);}
    void drawFastHLine(int x,int y,int w,uint16_t c){sprite.drawFastHLine(x,y-top,w,c);}
    void pushSprite(int x,int y){sprite.pushSprite(x,y+top);}
};
