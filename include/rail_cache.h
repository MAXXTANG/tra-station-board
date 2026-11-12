#pragma once
#include "rail_model.h"

namespace rail {
struct CacheMeta {std::string station,date;time_t checked=0;};
inline uint32_t cacheCrc(uint32_t crc,uint8_t byte){
    crc^=byte;for(int i=0;i<8;i++)crc=(crc>>1)^((crc&1)?0xedb88320U:0);return crc;
}
template<class Sink> struct CacheWriter {
    Sink &sink;uint32_t crc=0xffffffff;bool ok=true;
    explicit CacheWriter(Sink &output):sink(output){}
    size_t write(uint8_t byte){return write(&byte,1);}
    size_t write(const uint8_t *data,size_t n){
        size_t written=sink.write(data,n);if(written!=n)ok=false;
        for(size_t i=0;i<written;i++)crc=cacheCrc(crc,data[i]);return written;
    }
};
template<class Sink>
bool writeCache(Sink &sink,const CacheMeta &meta,const std::vector<Train> &trains){
    if(!midnightTaipei(meta.date) || meta.station.empty() || meta.station.size()>7 || trains.size()>1024)return false;
    CacheWriter<Sink> writer{sink};StaticJsonDocument<512> doc;
    doc["v"]=1;doc["station"]=meta.station;doc["date"]=meta.date;doc["checked"]=(int64_t)meta.checked;doc["count"]=trains.size();
    serializeJson(doc,writer);writer.write((uint8_t)'\n');
    for(const auto &t:trains){
        if(t.no.size()>12 || t.destination.size()>64 || t.type.size()>32 || t.code.size()>8 || (t.direction!=0 && t.direction!=1))return false;
        doc.clear();doc["no"]=t.no;doc["destination"]=t.destination;doc["type"]=t.type;doc["code"]=t.code;doc["departure"]=(int64_t)t.departure;doc["direction"]=t.direction;
        serializeJson(doc,writer);writer.write((uint8_t)'\n');
    }
    if(!writer.ok)return false;
    doc.clear();doc["crc"]=writer.crc^0xffffffff;
    size_t expected=measureJson(doc);size_t actual=serializeJson(doc,sink);
    return actual==expected && sink.write((uint8_t)'\n')==1;
}
template<class Input>
bool cacheLine(Input &input,std::string &line,uint32_t *crc){
    line.clear();int c;
    while((c=input.read())>=0){
        if(crc)*crc=cacheCrc(*crc,(uint8_t)c);
        if(c=='\n')return true;
        line+=(char)c;if(line.size()>1024)return false;
    }
    return false;
}
template<class Input>
bool readCache(Input &input,const std::string &station,CacheMeta &meta,std::vector<Train> &trains){
    uint32_t crc=0xffffffff;std::string line;StaticJsonDocument<512> doc;
    if(!cacheLine(input,line,&crc) || deserializeJson(doc,line) || !doc.is<JsonObject>())return false;
    CacheMeta next;next.station=doc["station"].as<std::string>();next.date=doc["date"].as<std::string>();next.checked=doc["checked"].as<int64_t>();
    int count=doc["count"] | -1;time_t day=midnightTaipei(next.date);
    if((doc["v"] | 0)!=1 || next.station!=station || !day || count<0 || count>1024 || next.checked<day-86400 || next.checked>day+3*86400)return false;
    std::vector<Train> rows;rows.reserve(count);
    for(int i=0;i<count;i++){
        doc.clear();if(!cacheLine(input,line,&crc) || deserializeJson(doc,line) || !doc.is<JsonObject>())return false;
        Train t;t.no=doc["no"].as<std::string>();t.destination=doc["destination"].as<std::string>();t.type=doc["type"].as<std::string>();t.code=doc["code"].as<std::string>();t.direction=doc["direction"] | -1;t.departure=doc["departure"].as<int64_t>();
        if(t.no.empty() || t.no.size()>12 || t.destination.empty() || t.destination.size()>64 || t.type.size()>32 || t.code.size()>8 || (t.direction!=0 && t.direction!=1) || t.departure<day || t.departure>=day+3*86400)return false;
        rows.push_back(t);
    }
    doc.clear();if(!cacheLine(input,line,nullptr) || deserializeJson(doc,line) || !doc["crc"].is<uint32_t>() || doc["crc"].as<uint32_t>()!=(crc^0xffffffff) || input.read()!=-1)return false;
    meta=next;trains.swap(rows);return true;
}
}
