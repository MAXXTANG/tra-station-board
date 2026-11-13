#pragma once
#include "board_profile.h"

namespace backlight {
constexpr int MIN=10, MAX=100, DEFAULT_PERCENT=100;
constexpr int CHANNEL=7, FREQUENCY=5000, RESOLUTION=8;
constexpr int LEFT=24, RIGHT=296;
constexpr int PRESETS[]={10,25,50,75,100};
constexpr int clamp(int value){return value<MIN?MIN:(value>MAX?MAX:value);}
constexpr int duty(int percent){return (clamp(percent)*255+50)/100;}
constexpr int sliderY(){return board::CYD?105:70;}
constexpr int presetY(){return board::CYD?140:94;}
constexpr int presetHeight(){return board::CYD?38:26;}
constexpr int recentY(){return 32+3*board::PICKER_STEP;}
constexpr int sliderX(int percent){return LEFT+(clamp(percent)-MIN)*(RIGHT-LEFT)/(MAX-MIN);}
constexpr int fromSlider(int x){return clamp(MIN+((x<LEFT?LEFT:(x>RIGHT?RIGHT:x))-LEFT)*(MAX-MIN)/(RIGHT-LEFT));}
inline int presetAt(int x,int y){
 if(y<presetY() || y>=presetY()+presetHeight())return -1;
 for(int i=0;i<5;i++)if(x>=8+i*62 && x<64+i*62)return i;
 return -1;
}
inline bool recentHit(int x,int y){return x>=6 && x<314 && y>=recentY() && y<recentY()+board::PICKER_STEP-3;}
}
