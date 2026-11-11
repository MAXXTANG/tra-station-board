#pragma once
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace rail {
struct Train {
    std::string no, destination, type, code;
    time_t departure = 0;
    int direction = -1;
};

inline int clockSeconds(const std::string &s) {
    if (s.size() != 5 && s.size() != 8) return -1;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 2 || i == 5) { if (s[i] != ':') return -1; }
        else if (s[i] < '0' || s[i] > '9') return -1;
    }
    int h = (s[0]-'0')*10+s[1]-'0', m = (s[3]-'0')*10+s[4]-'0';
    int sec = s.size()==8 ? (s[6]-'0')*10+s[7]-'0' : 0;
    return h<24 && m<60 && sec<60 ? h*3600+m*60+sec : -1;
}
inline time_t midnightTaipei(const std::string &date) {
    int y,m,d;
    if (date.size()!=10 || date[4]!='-' || date[7]!='-' ||
        sscanf(date.c_str(),"%d-%d-%d",&y,&m,&d)!=3 || y<2020 || m<1 || m>12) return 0;
    for(size_t i=0;i<date.size();++i)if(i!=4 && i!=7 && (date[i]<'0' || date[i]>'9'))return 0;
    const int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int maxDay = daysInMonth[m-1] + (m==2 && y%4==0 && (y%100!=0 || y%400==0));
    if (d<1 || d>maxDay) return 0;
    y-=m<=2; int era=(y>=0?y:y-399)/400; unsigned yo=y-era*400;
    unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
    long days=era*146097L+yo*365+yo/4-yo/100+doy-719468;
    return days*86400L-8*3600;
}
inline void classify(const std::string &carClass, Train &t) {
    if(carClass=="110G" || carClass=="110H") {t.code="3";t.type="自強3000";}
    else if(carClass=="1101") {t.code="1";t.type="太魯閣";}
    else if(carClass=="1107") {t.code="2";t.type="普悠瑪";}
    else if(carClass.compare(0,3,"110")==0) {t.code="3";t.type="自強";}
    else if(carClass.compare(0,3,"111")==0) {t.code="4";t.type="莒光";}
    else if(carClass=="1132") {t.code="10";t.type="區間快";}
    else if(carClass.compare(0,3,"113")==0) {t.code="6";t.type="區間";}
    else if(carClass.compare(0,3,"112")==0) {t.code="5";t.type="復興";}
    else if(carClass.compare(0,3,"114")==0 || carClass.compare(0,3,"115")==0) {t.code="7";t.type="普快";}
    else {t.code="0";t.type="列車";}
}
inline const char *shortType(const Train &t) {
    return t.type=="自強3000" ? "新自強" : t.type.c_str();
}

// ODS V1.6: LineDir 1=clockwise, 2=counterclockwise (not TDX's 0/1).
template<class Lookup>
inline void addOdsTrain(JsonObjectConst item, const std::string &station,
                        time_t serviceDay, Lookup lookup, std::vector<Train> &out) {
    std::string dir = item["LineDir"].as<std::string>();
    std::string no = item["Train"].as<std::string>();
    if ((dir!="1" && dir!="2") || no.empty() || !item["TimeInfos"].is<JsonArrayConst>()) return;
    std::vector<JsonObjectConst> stops;
    for (JsonObjectConst stop : item["TimeInfos"].as<JsonArrayConst>()) stops.push_back(stop);
    std::sort(stops.begin(),stops.end(),[](JsonObjectConst a,JsonObjectConst b){
        return atoi(a["Order"] | "0") < atoi(b["Order"] | "0");
    });
    if (stops.size()<2) return;
    int dayOffset=0,previous=-1;
    time_t departure=0; bool found=false;
    for(size_t i=0;i<stops.size();++i) {
        auto stop=stops[i];
        int arrival=clockSeconds(stop["ARRTime"].as<std::string>());
        int leaving=clockSeconds(stop["DEPTime"].as<std::string>());
        if(arrival>=0) {
            if(previous>=0 && previous-arrival>12*3600) ++dayOffset;
            previous=arrival;
        }
        if(leaving>=0) {
            if(previous>=0 && previous-leaving>12*3600) ++dayOffset;
            previous=leaving;
        }
        if(stop["Station"].as<std::string>()==station && leaving>=0 && i+1<stops.size()) {
            departure=serviceDay+dayOffset*86400+leaving; found=true;
        }
    }
    if(!found) return; // The terminating station has arrivals only.
    Train t;t.no=no;t.direction=dir=="1"?0:1;t.departure=departure;
    std::string destination=stops.back()["Station"].as<std::string>();
    const char *name=lookup(destination.c_str());
    t.destination=name && *name ? name : destination;
    classify(item["CarClass"].as<std::string>(),t);out.push_back(t);
}

struct DailyFile {std::string path,revision;};
inline bool findDailyFile(const std::string &html,const std::string &date,DailyFile &out) {
    if(!midnightTaipei(date)) return false;
    std::string name=date.substr(0,4)+date.substr(5,2)+date.substr(8,2)+".json";
    size_t pos=html.find(">"+name+"</a>");
    if(pos==std::string::npos) return false;
    size_t href=html.rfind("href=",pos);
    if(href==std::string::npos || href+6>=html.size()) return false;
    char quote=html[href+5];if(quote!='\"' && quote!='\'')return false;
    size_t end=html.find(quote,href+6);if(end==std::string::npos || end>=pos)return false;
    std::string path=html.substr(href+6,end-href-6);
    const std::string prefix="/tra-ods-web/ods/download/dataResource/exceptionDataResource/";
    if(path.compare(0,prefix.size(),prefix)!=0 || path.find_first_of("?#<>\r\n")!=std::string::npos) return false;
    size_t row=html.rfind("<tr",href),rowEnd=html.find("</tr>",pos);
    if(row==std::string::npos || rowEnd==std::string::npos || rowEnd-row>4096) return false;
    out.path=path;out.revision=html.substr(row,rowEnd+5-row);return true;
}

// Read one JSON value at a time, so a 2+ MB timetable never becomes a full JSON DOM.
template<class Input> class JsonReader {
    Input &input;int held=-2;
public:
    explicit JsonReader(Input &in):input(in){}
    int peek(){if(held==-2)held=input.read();return held;}
    int take(){int c=peek();held=-2;return c;}
    void space(){while(peek()==' ' || peek()=='\r' || peek()=='\n' || peek()=='\t')take();}
    bool consume(char c){space();if(peek()!=c)return false;take();return true;}
    bool value(std::string &s,size_t limit=65536) {
        space();s.clear();int c=peek();if(c<0)return false;
        if(c!='{' && c!='[' && c!='"') {
            while((c=peek())>=0 && c!=',' && c!=']' && c!='}' && c!=' ' && c!='\r' && c!='\n' && c!='\t') {
                s+=(char)take();if(s.size()>limit)return false;
            }
            return !s.empty();
        }
        bool text=false,escape=false;int depth=0;
        do {
            c=peek();if(c<0)return false;
            if(!text && depth==0 && !s.empty())break;
            c=take();s+=(char)c;if(s.size()>limit)return false;
            if(text){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='\"')text=false;}
            else if(c=='\"')text=true;
            else if(c=='{' || c=='[')++depth;
            else if(c=='}' || c==']')--depth;
            if(depth<0)return false;
        }while(text || depth>0);
        return !s.empty() && !text && depth==0;
    }
};
template<class Input,class Document,class Lookup>
bool parseOdsStream(Input &input,Document &doc,JsonVariantConst filter,
                    const std::string &station,const std::string &date,Lookup lookup,
                    std::vector<Train> &out,std::string &sourceUpdate,void (*tick)()=nullptr) {
    time_t day=midnightTaipei(date);if(!day)return false;
    JsonReader<Input> r(input);
    if(r.peek()==0xef){if(r.take()!=0xef || r.take()!=0xbb || r.take()!=0xbf)return false;}
    if(!r.consume('{'))return false;
    std::vector<Train> next;bool found=false;size_t count=0;std::string update;
    while(true) {
        std::string key,value;DynamicJsonDocument scalar(256);
        if(!r.value(value,128) || deserializeJson(scalar,value) || !scalar.is<const char*>())return false;
        key=scalar.as<std::string>();if(!r.consume(':'))return false;
        if(key=="TrainInfos") {
            if(found || !r.consume('['))return false;found=true;r.space();
            if(r.peek()!=']')while(true) {
                if(!r.value(value) || value.empty() || value[0]!='{')return false;
                doc.clear();
                if(deserializeJson(doc,value,DeserializationOption::Filter(filter)) || !doc.template is<JsonObject>())return false;
                addOdsTrain(doc.template as<JsonObjectConst>(),station,day,lookup,next);++count;if(tick)tick();
                r.space();if(r.peek()==']')break;if(!r.consume(','))return false;
            }
            if(!r.consume(']'))return false;
        } else {
            if(!r.value(value,1024))return false;
            if(key=="UpdateTime") {
                scalar.clear();if(deserializeJson(scalar,value) || !scalar.is<const char*>())return false;
                update=scalar.as<std::string>();
            }else{scalar.clear();if(deserializeJson(scalar,value))return false;}
        }
        r.space();if(r.peek()=='}'){r.take();break;}if(!r.consume(','))return false;
    }
    if(!found || count==0)return false;
    out.swap(next);sourceUpdate=update;return true;
}
inline std::vector<Train> upcoming(const std::vector<Train> &all,int dir,time_t now) {
    std::vector<Train> rows;
    for(const auto &t:all)if(t.direction==dir && t.departure>=now-45 && t.departure<now+86400)rows.push_back(t);
    std::sort(rows.begin(),rows.end(),[](const Train&a,const Train&b){return a.departure<b.departure;});return rows;
}
}
