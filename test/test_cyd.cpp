#include <cassert>
#include <sstream>
#include <iostream>
#include "touch_calibration.h"
#include "rail_cache.h"

struct MemoryFile {
    std::stringstream stream;
    size_t write(uint8_t c){stream.put((char)c);return stream?1:0;}
    size_t write(const uint8_t *p,size_t n){stream.write((const char*)p,n);return stream?n:0;}
    int read(){return stream.get();}
};
int main(){
    // Swapped and inverted resistive axes must map to the visible targets,
    // including 180-degree mode, without board-specific hardcoded ranges.
    display::Point raw[]={{3800,250},{3800,3850},{200,250}};
    display::Point target[]={{20,60},{300,60},{20,220}};
    display::Calibration cal;assert(display::solveCalibration(raw,target,cal));
    int x,y;assert(cal.map(200,3850,false,x,y)&&x==300&&y==220);
    assert(cal.map(200,3850,true,x,y)&&x==19&&y==19);
    display::Point bad[]={{10,10},{20,20},{30,30}};assert(!display::solveCalibration(bad,target,cal));
    auto day=rail::midnightTaipei("2026-11-13");
    rail::CacheMeta meta;meta.station="7210";meta.date="2026-11-13";meta.checked=day+36000;
    rail::Train t;t.no="4238";t.destination="蘇澳";t.type="區間";t.code="6";t.direction=0;t.departure=day+21*3600;
    std::vector<rail::Train> expected{t},actual;
    MemoryFile file;assert(rail::writeCache(file,meta,expected));auto encoded=file.stream.str();
    rail::CacheMeta read;assert(rail::readCache(file,"7210",read,actual));assert(actual.size()==1&&actual[0].no==t.no&&actual[0].destination==t.destination);
    auto testBad=[&](std::string blob,const std::string &station="7210"){
        MemoryFile input;input.stream.str(blob);auto saved=actual;
        assert(!rail::readCache(input,station,read,actual));assert(actual.size()==saved.size()&&actual[0].no==saved[0].no);
    };
    testBad(encoded,"7190");testBad(encoded.substr(0,encoded.size()-3));
    auto corrupt=encoded;corrupt[corrupt.find("4238")]='9';testBad(corrupt);
    testBad(encoded+"garbage");
    std::cout<<"PASS: CYD affine touch calibration, 180-degree mapping, degenerate samples, cache roundtrip, wrong station, truncation and checksum rejection\n";
}
