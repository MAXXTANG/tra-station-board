#pragma once
#ifdef TRA_BOARD_CYD
#include <SPI.h>
#include <algorithm>

// XPT2046 is on a different SPI bus from the LCD on the standard CYD.
class CydTouch {
    SPIClass spi{VSPI};uint32_t lastRead=0;
    int sample(uint8_t command){
        spi.transfer(command);return (spi.transfer16(0)>>3)&4095;
    }
public:
    void begin(){pinMode(33,OUTPUT);digitalWrite(33,HIGH);pinMode(36,INPUT);spi.begin(25,39,32,33);}
    bool raw(int &x,int &y){
        if(millis()-lastRead<20 || digitalRead(36)!=LOW)return false;lastRead=millis();
        spi.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0));digitalWrite(33,LOW);
        int z1=sample(0xb0),z2=sample(0xc0),xs[5],ys[5];
        for(int i=0;i<5;++i){xs[i]=sample(0xd0);ys[i]=sample(0x90);}
        // Final conversion powers down the ADC and enables PENIRQ.
        sample(0x80);digitalWrite(33,HIGH);spi.endTransaction();
        if(z1+4095-z2<400)return false;
        std::sort(xs,xs+5);std::sort(ys,ys+5);x=xs[2];y=ys[2];
        return x>0 && x<4095 && y>0 && y<4095;
    }
    bool released()const{return digitalRead(36)!=LOW;}
};
#endif
