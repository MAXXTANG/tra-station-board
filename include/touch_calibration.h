#pragma once
#include <cmath>
#include <initializer_list>
#include "board_profile.h"

namespace display {
struct Point {float x,y;Point(float px=0,float py=0):x(px),y(py){}};
struct Calibration {
    float a=0,b=0,c=0,d=0,e=0,f=0;
    bool valid=false;
    bool map(int rawX,int rawY,bool upsideDown,int &x,int &y)const {
        if(!valid)return false;
        x=std::lround(a*rawX+b*rawY+c);y=std::lround(d*rawX+e*rawY+f);
        if(x<0 || x>=board::WIDTH || y<0 || y>=board::HEIGHT)return false;
        if(upsideDown){x=board::WIDTH-1-x;y=board::HEIGHT-1-y;}return true;
    }
};
inline bool solveCalibration(const Point raw[3],const Point target[3],Calibration &out){
    const float x0=raw[0].x,y0=raw[0].y,x1=raw[1].x,y1=raw[1].y,x2=raw[2].x,y2=raw[2].y;
    const float det=x0*(y1-y2)+x1*(y2-y0)+x2*(y0-y1);
    if(!std::isfinite(det) || std::fabs(det)<1000)return false;
    auto fit=[&](float z0,float z1,float z2,float &a,float &b,float &c){
        a=(z0*(y1-y2)+z1*(y2-y0)+z2*(y0-y1))/det;
        b=(z0*(x2-x1)+z1*(x0-x2)+z2*(x1-x0))/det;
        c=(z0*(x1*y2-x2*y1)+z1*(x2*y0-x0*y2)+z2*(x0*y1-x1*y0))/det;
    };
    Calibration next;fit(target[0].x,target[1].x,target[2].x,next.a,next.b,next.c);
    fit(target[0].y,target[1].y,target[2].y,next.d,next.e,next.f);
    for(float v:{next.a,next.b,next.c,next.d,next.e,next.f})if(!std::isfinite(v))return false;
    next.valid=true;out=next;return true;
}
}
