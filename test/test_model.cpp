#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include "rail_model.h"
#include "ods_stream.h"
#include "stations.h"

struct Input {
    std::istream &stream;
    int read() { return stream.get(); }
};
const char *lookup(const char *id) {
    for(const auto &s:STATIONS)if(std::string(id)==s.id)return s.name;
    return "";
}
bool parse(std::istream &stream,const std::string &station,const std::string &date,
           std::vector<rail::Train> &out) {
    Input input{stream};
#ifdef TEST_COMPACT_ODS
    std::string source;
    return rail::parseOdsCompact(input,station,date,lookup,out,source);
#else
    DynamicJsonDocument doc(98304),filter(1024);
    filter["Train"]=true;filter["LineDir"]=true;filter["CarClass"]=true;
    JsonObject stop=filter["TimeInfos"].to<JsonArray>().createNestedObject();
    for(const char *field:{"Station","Order","ARRTime","DEPTime"})stop[field]=true;
    std::string source;
    return rail::parseOdsStream(input,doc,filter.as<JsonVariantConst>(),station,date,lookup,out,source);
#endif
}
bool parse(const std::string &s,std::vector<rail::Train> &out) {
    std::istringstream stream(s);return parse(stream,"7210","2026-11-11",out);
}
int main(int argc,char **argv) {
    auto day=rail::midnightTaipei("2026-11-11");assert(day>0);
    assert(!rail::midnightTaipei("2026-02-29"));assert(!rail::midnightTaipei("2026-11-1x"));
    assert(rail::midnightTaipei("2024-02-29")>0);
    assert(rail::clockSeconds("24:00:00")==-1);assert(rail::clockSeconds("12:99")==-1);
    assert(rail::clockSeconds("12:30:45")==45045);
    const std::string fixture=R"({"UpdateTime":"2026-11-10 18:02:42","Count":12,"TrainInfos":[{"Train":"4238","LineDir":"1","CarClass":"1131","Note":"quote \" and brace } inside a string","TimeInfos":[{"Station":"7360","Order":"1","ARRTime":"21:00:00","DEPTime":"21:00:00"},{"Station":"7210","Order":"2","ARRTime":"21:21:00","DEPTime":"21:22:30"},{"Station":"7120","Order":"3","ARRTime":"22:00:00","DEPTime":"22:00:00"}]},{"Train":"445","LineDir":"2","CarClass":"110H","TimeInfos":[{"Station":"7000","Order":"1","ARRTime":"23:40:00","DEPTime":"23:40:00"},{"Station":"7210","Order":"2","ARRTime":"23:59:00","DEPTime":"00:02:00"},{"Station":"1040","Order":"3","ARRTime":"00:40:00","DEPTime":"00:40:00"}]},{"Train":"999","LineDir":"1","CarClass":"1131","TimeInfos":[{"Station":"1000","Order":"1","ARRTime":"20:00:00","DEPTime":"20:00:00"},{"Station":"7210","Order":"2","ARRTime":"22:00:00","DEPTime":"22:00:00"}]}]})";
    std::vector<rail::Train> rows;assert(parse(fixture,rows));assert(rows.size()==2);
    assert(rows[0].no=="4238" && rows[0].direction==0 && rows[0].destination=="蘇澳");
    assert(rows[0].departure==day+21*3600+22*60+30);
    assert(rows[1].direction==1 && rows[1].destination=="樹林");
    assert(rows[1].departure==day+86400+120);assert(std::string(rail::shortType(rows[1]))=="新自強");
    auto saved=rows;
    assert(!parse(fixture.substr(0,fixture.size()-2),rows));assert(rows.size()==saved.size());
    assert(!parse("<html>Maintenance</html>",rows));assert(!parse("{\"TrainInfos\":null}",rows));
    assert(!parse("{\"TrainInfos\":[]}",rows));assert(rows[0].no=="4238");
    assert(parse(std::string("\xef\xbb\xbf")+fixture,rows));
    assert(rail::upcoming(rows,0,day+21*3600).size()==1);
    assert(rail::upcoming(rows,0,day+22*3600).empty());
    rail::Train type;rail::classify("1101",type);assert(type.type=="太魯閣");
    rail::classify("1102",type);assert(type.type=="自強");
    rail::classify("1107",type);assert(type.type=="普悠瑪");
    rail::classify("1132",type);assert(type.type=="區間快");
    const std::string html="<table><tr><td>2026-11-11 03:40</td><td><a href=\"/tra-ods-web/ods/download/dataResource/exceptionDataResource/abc123\">20261111.json</a></td></tr></table>";
    rail::DailyFile file;assert(rail::findDailyFile(html,"2026-11-11",file));
    assert(file.path.find("abc123")!=std::string::npos && file.revision.find("03:40")!=std::string::npos);
    assert(!rail::findDailyFile(html,"2026-11-12",file));
    std::string foreign=html;foreign.replace(foreign.find("/tra-ods-web/ods/download/dataResource/exceptionDataResource/abc123"),76,"https://example.com/file");
    assert(!rail::findDailyFile(foreign,"2026-11-11",file));
    std::cout<<"PASS: ODS parsing, direction codes, destinations, terminal exclusion, seconds, midnight dwell, malformed/truncated data, cache preservation and catalog validation\n";
    if(argc>1) {
        std::ifstream full(argv[1]);assert(full);std::vector<rail::Train> actual;
        assert(parse(full,"7210","2026-11-11",actual));
        assert(actual.size()>30);
        auto it=std::find_if(actual.begin(),actual.end(),[](const auto&t){return t.no=="4238";});
        assert(it!=actual.end() && it->destination=="蘇澳" && it->direction==0 && it->departure==day+21*3600+22*60);
        std::cout<<"PASS: complete official daily file streamed, Jiaoxi departures="<<actual.size()<<"\n";
    }
}
