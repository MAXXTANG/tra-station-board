#pragma once
#include <ArduinoJson.h>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
namespace rail {
struct Train {
 std::string no,destination,type,code,platform;
 time_t departure=0, liveAt=0;
 int direction=-1, delay=0, status=-1;
 bool cancelled=false;
};
inline time_t isoTime(const char *s) {
 if(!s || strlen(s)<19) return 0;
 int y,m,d,h,n,sec,oh=0,om=0; char sign='+';
 if(sscanf(s,"%d-%d-%dT%d:%d:%d",&y,&m,&d,&h,&n,&sec)!=6) return 0;
 // civil date to UTC, independent of the host timezone.
 y-=m<=2; int era=(y>=0?y:y-399)/400; unsigned yo=y-era*400;
 unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
 long days=era*146097L+yo*365+yo/4-yo/100+doy-719468;
 if(strlen(s)>=25) {sign=s[19];sscanf(s+20,"%d:%d",&oh,&om);}
 else if(s[19]!='Z') return 0;
 long off=(oh*60+om)*60; if(sign=='-') off=-off;
 return days*86400L+h*3600+n*60+sec-off;
}
inline time_t departureAt(const std::string &date,const char *clock) {
 if(!clock || strlen(clock)<5) return 0;
 return isoTime((date+"T"+std::string(clock).substr(0,5)+":00+08:00").c_str());
}
inline bool fresh(time_t stamp,time_t now) {return stamp>0 && now-stamp>=-60 && now-stamp<=300;}
inline std::string remark(const Train &t,time_t now,bool connected) {
 if(t.cancelled) return "停駛";
 if(!connected || !fresh(t.liveAt,now)) return "待更新";
 if(t.status==2) return "取消";
 if(t.delay>0) return "晚"+std::to_string(t.delay)+"分";
 if(t.status==0 || (t.status<0 && t.delay==0)) return "準點";
 return "待更新";
}
inline bool parseSchedule(JsonVariantConst root,const std::string &station,const std::string &date,std::vector<Train> &out) {
 if(root["TrainDate"].as<std::string>()!=date || !root["StationTimetables"].is<JsonArrayConst>()) return false;
 std::vector<Train> next; bool matched=false;
 for(JsonObjectConst group:root["StationTimetables"].as<JsonArrayConst>()) {
  if(group["StationID"].as<std::string>()!=station || !group["TimeTables"].is<JsonArrayConst>()) continue;
  matched=true;
  int dir=group["Direction"] | -1;
  if(dir<0 || dir>1) continue;
  for(JsonObjectConst item:group["TimeTables"].as<JsonArrayConst>()) {
   Train t;t.no=item["TrainNo"].as<std::string>(); t.direction=dir;
   t.destination=item["DestinationStationName"]["Zh_tw"].as<std::string>();
   t.type=item["TrainTypeName"]["Zh_tw"].as<std::string>();t.code=item["TrainTypeCode"].as<std::string>();
   t.departure=departureAt(date,item["DepartureTime"] | "");
   t.cancelled=(item["SuspendedFlag"] | 0)==1;
   if(!t.no.empty() && t.departure>0) next.push_back(t);
  }
 }
 if(!matched)return false;
 out.swap(next); return true;
}
inline bool mergeLive(JsonVariantConst root,const std::string &station,const std::string &date,time_t now,std::vector<Train> &trains) {
 if(!root["StationLiveBoards"].is<JsonArrayConst>()) return false;
 for(auto &t:trains){t.liveAt=0;t.status=-1;t.delay=0;}
 for(JsonObjectConst item:root["StationLiveBoards"].as<JsonArrayConst>()) {
  if(item["StationID"].as<std::string>()!=station)continue;
  int dir=item["Direction"] | -1;
  std::string no=item["TrainNo"].as<std::string>();
  time_t updated=isoTime(item["UpdateTime"] | "");
  if(no.empty() || dir<0 || dir>1 || !fresh(updated,now))continue;
  time_t liveDep=departureAt(date,item["ScheduleDepartureTime"] | "");
  if(liveDep-now>12*3600)liveDep-=86400;
  auto it=std::find_if(trains.begin(),trains.end(),[&](const Train &t){return t.no==no && t.direction==dir && (!liveDep || std::abs(t.departure-liveDep)<3600);});
  if(it==trains.end()) {
   Train t; t.no=no;t.direction=dir;t.departure=liveDep;
   if(!t.departure)continue;
   // Around midnight a live row with a late-evening departure belongs to yesterday.
   if(t.departure-now>12*3600)t.departure-=86400;
   t.destination=item["EndingStationName"]["Zh_tw"].as<std::string>();
   t.type=item["TrainTypeName"]["Zh_tw"].as<std::string>();t.code=item["TrainTypeCode"].as<std::string>();
   trains.push_back(t);it=trains.end()-1;
  }
  if(item["DelayTime"].isNull())continue;
  it->delay=std::max(0,item["DelayTime"].as<int>());it->status=item["RunningStatus"] | -1;
  it->liveAt=updated;it->platform=item["Platform"].as<std::string>();
 }
 return true;
}
inline std::vector<Train> upcoming(const std::vector<Train> &all,int dir,time_t now) {
 std::vector<Train> rows;
 for(const auto &t:all) {
  time_t due=t.departure+(fresh(t.liveAt,now)?t.delay*60:0);
  if(t.direction==dir && due>=now-45 && due<now+86400)rows.push_back(t);
 }
 std::sort(rows.begin(),rows.end(),[](const Train&a,const Train&b){return a.departure<b.departure;});return rows;
}
inline const char* shortType(const Train&t){
 if(t.type.find("3000")!=std::string::npos)return "新自強";
 if(t.code=="1")return "太魯閣";if(t.code=="2")return "普悠瑪";
 if(t.code=="3")return "自強";if(t.code=="4")return "莒光";
 if(t.code=="6")return "區間";if(t.code=="10")return "區間快";
 return t.type.empty()?"列車":t.type.c_str();
}
}
