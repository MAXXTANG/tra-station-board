#include <cassert>
#include <cstdlib>
#include <iostream>
#include "brightness.h"

int main(){
 assert(backlight::duty(10)>0 && backlight::duty(100)==255);
 assert(backlight::clamp(0)==10 && backlight::clamp(500)==100);
 int previous=-1;
 for(int p=10;p<=100;p++){int d=backlight::duty(p);assert(d>previous && d<=255);previous=d;}
 assert(backlight::fromSlider(-100)==10 && backlight::fromSlider(999)==100);
 for(int p:backlight::PRESETS){assert(std::abs(backlight::fromSlider(backlight::sliderX(p))-p)<=1);}
 for(int i=0;i<5;i++)assert(backlight::presetAt(8+i*62+28,backlight::presetY()+12)==i);
 assert(backlight::presetAt(65,backlight::presetY()+12)==-1);
 assert(backlight::presetAt(20,backlight::presetY()-1)==-1);
 assert(backlight::recentHit(160,backlight::recentY()+3));
 assert(!backlight::recentHit(160,board::PICKER_BOTTOM));
 assert(backlight::recentY()+board::PICKER_STEP-3<=board::PICKER_BOTTOM);
 assert(backlight::presetY()+backlight::presetHeight()<board::PICKER_BOTTOM);
 std::cout<<"PASS: safe PWM range, monotonic levels, slider bounds and touch targets ("<<board::HEIGHT<<"px)\n";
}
