#pragma once
#include <Arduino.h>
#include <freertos/semphr.h>
#include "board_profile.h"
#include "frame_buffer.h"
#include "rail_cache.h"

class SemaphoreLease {
    SemaphoreHandle_t semaphore;bool locked;
public:
    SemaphoreLease(SemaphoreHandle_t s,TickType_t wait):semaphore(s),locked(xSemaphoreTake(s,wait)==pdTRUE){}
    ~SemaphoreLease(){if(locked)xSemaphoreGive(semaphore);}
    explicit operator bool()const{return locked;}
    SemaphoreLease(const SemaphoreLease&)=delete;
    SemaphoreLease& operator=(const SemaphoreLease&)=delete;
};

// ASCII row packets avoid treating diagnostic text as image data. Each row is
// RLE RGB565, CRC32 checked and acknowledged before sending the next one.
// A missing host or bad packet aborts quickly and returns normal UI control.
template<class Render>
bool captureRows(BoardFrame &frame,Render render){
    Serial.printf("CAPTURE_RLE565 %d %d 1\n",board::WIDTH,board::HEIGHT);
    uint8_t packed[board::WIDTH*3];char hex[board::WIDTH*6+1];
    const char digits[]="0123456789abcdef";
    for(int band=0;band<board::HEIGHT;band+=BoardFrame::BAND_HEIGHT){
        frame.band(band);render();
        for(int y=band;y<std::min(band+BoardFrame::BAND_HEIGHT,board::HEIGHT);y++){
            size_t n=0;
            for(int x=0;x<board::WIDTH;){
                uint16_t color=frame.readPixel(x,y);int run=1;
                while(x+run<board::WIDTH && run<255 && frame.readPixel(x+run,y)==color)++run;
                packed[n++]=(uint8_t)run;packed[n++]=color>>8;packed[n++]=color&255;x+=run;
            }
            uint32_t crc=0xffffffff;
            for(size_t i=0;i<n;i++){crc=rail::cacheCrc(crc,packed[i]);hex[i*2]=digits[packed[i]>>4];hex[i*2+1]=digits[packed[i]&15];}
            hex[n*2]=0;crc^=0xffffffff;bool accepted=false;
            for(int attempt=0;attempt<3 && !accepted;attempt++){
                Serial.printf("ROW %d %s %08lx\n",y,hex,(unsigned long)crc);Serial.flush();
                char ack[16]={};size_t used=0;uint32_t started=millis();bool ended=false;
                while(millis()-started<2500 && !ended){
                    while(Serial.available()){
                        char c=Serial.read();if(c=='\r')continue;
                        if(c=='\n'){ended=true;break;}
                        if(used<sizeof(ack)-1)ack[used++]=c;
                    }
                    if(!ended)delay(1);
                }
                if(ended && ack[0]=='A' && atoi(ack+1)==y)accepted=true;
            }
            if(!accepted){Serial.println("CAPTURE_ABORT");return false;}
        }
    }
    Serial.println("CAPTURE_END");return true;
}
