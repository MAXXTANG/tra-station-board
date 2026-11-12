#pragma once
#include "rail_model.h"
#include <cstring>

namespace rail {
// The CYD has no PSRAM in its standard configuration. Keep one small stop
// object and compact stop records, rather than a JSON DOM for a whole train.
struct OdsStop {
    char station[8] = {};
    int order = 0, arrival = -1, departure = -1;
};
struct ParseStats { size_t trains=0, maxStops=0, maxValueBytes=0; };

template<class Reader>
bool jsonText(Reader &r, std::string &out, size_t limit=128) {
    std::string encoded;
    StaticJsonDocument<256> doc;
    if(!r.value(encoded,limit) || deserializeJson(doc,encoded) || !doc.is<const char*>()) return false;
    out=doc.as<std::string>();return true;
}

template<class Reader,class Lookup>
bool readCompactTrain(Reader &r,const std::string &selected,time_t day,Lookup lookup,
                      std::vector<Train> &out,ParseStats &stats) {
    if(!r.consume('{'))return false;
    std::string no,dir,kind;std::vector<OdsStop> stops;stops.reserve(32);
    bool haveStops=false;r.space();
    if(r.peek()=='}'){r.take();return true;}
    while(true) {
        std::string key,value;if(!jsonText(r,key) || !r.consume(':'))return false;
        if(key=="TimeInfos") {
            if(haveStops || !r.consume('['))return false;haveStops=true;r.space();
            if(r.peek()!=']')while(true) {
                if(stops.size()>=512 || !r.value(value,2048))return false;
                stats.maxValueBytes=std::max(stats.maxValueBytes,value.size());
                StaticJsonDocument<512> doc;StaticJsonDocument<128> filter;
                for(const char *field:{"Station","Order","ARRTime","DEPTime"})filter[field]=true;
                if(deserializeJson(doc,value,DeserializationOption::Filter(filter)) || !doc.is<JsonObject>())return false;
                const char *id=doc["Station"] | "";
                if(!*id || strlen(id)>=sizeof(OdsStop::station))return false;
                OdsStop stop;strcpy(stop.station,id);
                stop.order=atoi(doc["Order"] | "0");if(stop.order<=0)return false;
                stop.arrival=clockSeconds(doc["ARRTime"].as<std::string>());
                stop.departure=clockSeconds(doc["DEPTime"].as<std::string>());
                stops.push_back(stop);
                r.space();if(r.peek()==']')break;if(!r.consume(','))return false;
            }
            if(!r.consume(']'))return false;
        } else if(key=="Train" || key=="LineDir" || key=="CarClass") {
            if(!jsonText(r,value,128))return false;
            if(key=="Train")no=value;else if(key=="LineDir")dir=value;else kind=value;
        } else {
            if(!r.value(value,8192))return false;
            // Validate skipped values too; a partial or malformed file must not
            // replace a previously valid timetable.
            DynamicJsonDocument doc(12288);
            if(deserializeJson(doc,value))return false;
        }
        r.space();if(r.peek()=='}'){r.take();break;}if(!r.consume(','))return false;
    }
    ++stats.trains;stats.maxStops=std::max(stats.maxStops,stops.size());
    if(no.empty() || (dir!="1" && dir!="2") || stops.size()<2)return true;
    std::sort(stops.begin(),stops.end(),[](const OdsStop&a,const OdsStop&b){return a.order<b.order;});
    int previous=-1,offset=0;time_t departure=0;bool found=false;
    for(size_t i=0;i<stops.size();++i) {
        const auto &s=stops[i];
        if(i && stops[i-1].order==s.order)return false;
        for(int seconds:{s.arrival,s.departure})if(seconds>=0){
            if(previous>=0 && previous-seconds>12*3600)++offset;previous=seconds;
        }
        if(selected==s.station && s.departure>=0 && i+1<stops.size()){
            departure=day+offset*86400+s.departure;found=true;
        }
    }
    if(found) {
        if(out.size()>=1024)return false;
        Train t;t.no=no;t.direction=dir=="1"?0:1;t.departure=departure;
        const char *name=lookup(stops.back().station);
        t.destination=name&&*name?name:stops.back().station;classify(kind,t);out.push_back(t);
    }
    return true;
}

template<class Input,class Lookup>
bool parseOdsCompact(Input &input,const std::string &station,const std::string &date,
                     Lookup lookup,std::vector<Train> &out,std::string &sourceUpdate,
                     ParseStats *report=nullptr,void (*tick)()=nullptr) {
    time_t day=midnightTaipei(date);if(!day)return false;
    JsonReader<Input> r(input);
    if(r.peek()==0xef && (r.take()!=0xef || r.take()!=0xbb || r.take()!=0xbf))return false;
    if(!r.consume('{'))return false;
    std::vector<Train> next;std::string update;bool found=false;ParseStats stats;
    while(true) {
        std::string key,value;if(!jsonText(r,key) || !r.consume(':'))return false;
        if(key=="TrainInfos") {
            if(found || !r.consume('['))return false;found=true;r.space();
            if(r.peek()!=']')while(true) {
                if(!readCompactTrain(r,station,day,lookup,next,stats))return false;
                if(tick)tick();r.space();if(r.peek()==']')break;if(!r.consume(','))return false;
            }
            if(!r.consume(']'))return false;
        } else {
            if(!r.value(value,1024))return false;
            DynamicJsonDocument doc(2048);if(deserializeJson(doc,value))return false;
            if(key=="UpdateTime"){
                if(!doc.is<const char*>())return false;update=doc.as<std::string>();
            }
        }
        r.space();if(r.peek()=='}'){r.take();break;}if(!r.consume(','))return false;
    }
    if(!found || !stats.trains)return false;
    out.swap(next);sourceUpdate=update;if(report)*report=stats;return true;
}
}
